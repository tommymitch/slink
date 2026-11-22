// slink_bench — measures engine cost against the audio-callback deadline.
//
// For every (engine, physics rate, block size) combination it renders a few
// seconds of audio block-by-block, timing each process() call exactly as a host
// would call processBlock(). Load is reported as a percentage of the real-time
// budget (blockSize / sampleRate): 100% means the callback only just meets its
// deadline. The worst-case block matters most — a single late block is an
// audible dropout — so p99 and max are reported alongside the mean.
//
// Results are printed as a table and appended to bench/results.csv so each
// development stage can be compared with the last.

#include <JuceHeader.h>

#include "Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace slink::bench
{
#if SLINK_HAS_REFMODEL
std::unique_ptr<Engine> createRefModelEngine();
#endif
std::unique_ptr<Engine> createSlinkCoreEngine();
std::unique_ptr<Engine> createSlinkCoreTimeScaledEngine();

std::vector<EngineInfo> allEngines()
{
    std::vector<EngineInfo> engines;

   #if SLINK_HAS_REFMODEL
    engines.push_back ({ "refmodel", "Reference model: original OO mesh (50x15 spider-cross), Heun integrator", createRefModelEngine });
   #endif

    engines.push_back ({ "slink-core",    "SlinkCore SoA mesh (50x15 spider-cross), legacy integrator",      createSlinkCoreEngine });
    engines.push_back ({ "slink-core-ts", "SlinkCore SoA mesh (50x15 spider-cross), time-scaled integrator", createSlinkCoreTimeScaledEngine });

    return engines;
}
} // namespace slink::bench

using namespace slink::bench;

namespace
{

struct Options
{
    double sampleRate = 48000.0;
    double seconds = 5.0;
    std::vector<int> blockSizes { 64, 128, 256, 512 };
    std::vector<double> physicsRates { 60.0, 1000.0, 4000.0, 48000.0 };
    std::vector<std::string> engines;   // empty = all
    std::string label = "unlabelled";
    std::string csvPath = std::string (SLINK_SOURCE_DIR) + "/bench/results.csv";
    bool writeCsv = true;
};

struct BenchResult
{
    std::string engine;
    double physicsRate = 0;
    int blockSize = 0;
    double budgetUs = 0, meanUs = 0, p99Us = 0, maxUs = 0;
    bool stable = true;
};

template <typename T>
std::vector<T> parseList (const std::string& s)
{
    std::vector<T> out;
    std::stringstream ss (s);
    std::string item;
    while (std::getline (ss, item, ','))
        if (! item.empty())
            out.push_back ((T) std::stod (item));
    return out;
}

void printUsage()
{
    std::cout <<
        "usage: slink_bench [options]\n"
        "  --label <text>        tag this run in results.csv (e.g. \"stage1-soa\")\n"
        "  --engines a,b         engines to run (default: all)\n"
        "  --rates 60,1000,...   physics update rates in Hz (sample rate = audio rate)\n"
        "  --blocks 64,128,...   audio block sizes in samples\n"
        "  --sr <hz>             sample rate (default 48000)\n"
        "  --seconds <s>         audio rendered per configuration (default 5)\n"
        "  --no-csv              don't append to results.csv\n"
        "  --list                list engines\n";
}

bool parseArgs (int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&]() -> std::string
        {
            if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit (1); }
            return argv[++i];
        };

        if      (a == "--label")   o.label = next();
        else if (a == "--engines") { std::stringstream ss (next()); std::string e; while (std::getline (ss, e, ',')) o.engines.push_back (e); }
        else if (a == "--rates")   o.physicsRates = parseList<double> (next());
        else if (a == "--blocks")  o.blockSizes = parseList<int> (next());
        else if (a == "--sr")      o.sampleRate = std::stod (next());
        else if (a == "--seconds") o.seconds = std::stod (next());
        else if (a == "--no-csv")  o.writeCsv = false;
        else if (a == "--list")
        {
            for (auto& e : allEngines())
                std::cout << e.name << "  —  " << e.description << "\n";
            std::exit (0);
        }
        else { printUsage(); return false; }
    }
    return true;
}

std::string gitCommit()
{
    std::string cmd = std::string ("git -C \"") + SLINK_SOURCE_DIR + "\" rev-parse --short HEAD 2>/dev/null";
    std::string out;
    if (auto* pipe = popen (cmd.c_str(), "r"))
    {
        char buf[64];
        while (fgets (buf, sizeof buf, pipe))
            out += buf;
        pclose (pipe);
    }
    out.erase (std::remove (out.begin(), out.end(), '\n'), out.end());
    return out.empty() ? "nogit" : out;
}

std::string timestamp()
{
    auto t = std::time (nullptr);
    char buf[32];
    std::strftime (buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", std::localtime (&t));
    return buf;
}

BenchResult runOne (const EngineInfo& info, const Options& o, double physicsRate, int blockSize)
{
    using clock = std::chrono::steady_clock;

    juce::ScopedNoDenormals noDenormals;   // as in a real processBlock

    auto engine = info.create();
    engine->prepare (o.sampleRate, physicsRate, blockSize);

    std::vector<float> buffer ((size_t) blockSize, 0.0f);

    const auto numBlocks = (int) std::ceil (o.seconds * o.sampleRate / blockSize);
    const auto warmupBlocks = std::max (8, numBlocks / 50);
    const auto blocksPerNote = std::max (1, (int) (0.25 * o.sampleRate / blockSize));   // a note every 250 ms
    static constexpr int notes[] = { 48, 55, 60, 64, 67, 72, 60, 52 };

    std::vector<double> timesUs;
    timesUs.reserve ((size_t) numBlocks);

    bool stable = true;
    int noteIndex = 0;

    for (int b = 0; b < warmupBlocks + numBlocks; ++b)
    {
        if (b % blocksPerNote == 0)
            engine->noteOn (notes[noteIndex++ % std::size (notes)], 0.8f);

        const auto start = clock::now();
        engine->process (buffer.data(), blockSize);
        const auto end = clock::now();

        if (b >= warmupBlocks)
            timesUs.push_back (std::chrono::duration<double, std::micro> (end - start).count());

        for (auto s : buffer)
            if (! std::isfinite (s))
                stable = false;
    }

    std::sort (timesUs.begin(), timesUs.end());

    BenchResult r;
    r.engine = info.name;
    r.physicsRate = physicsRate;
    r.blockSize = blockSize;
    r.budgetUs = 1.0e6 * blockSize / o.sampleRate;
    r.meanUs = std::accumulate (timesUs.begin(), timesUs.end(), 0.0) / (double) timesUs.size();
    r.p99Us = timesUs[(size_t) (0.99 * (double) (timesUs.size() - 1))];
    r.maxUs = timesUs.back();
    r.stable = stable;
    return r;
}

std::string pct (double us, double budget)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%7.2f%%", 100.0 * us / budget);
    return buf;
}

void appendCsv (const Options& o, const std::vector<BenchResult>& results)
{
    const bool exists = std::filesystem::exists (o.csvPath);
    std::ofstream f (o.csvPath, std::ios::app);
    if (! f)
    {
        std::cerr << "could not write " << o.csvPath << "\n";
        return;
    }

    if (! exists)
        f << "timestamp,label,commit,engine,sample_rate,physics_rate,block,budget_us,mean_us,p99_us,max_us,mean_pct,p99_pct,max_pct,stable\n";

    const auto ts = timestamp();
    const auto commit = gitCommit();

    for (auto& r : results)
        f << ts << ',' << o.label << ',' << commit << ',' << r.engine << ',' << o.sampleRate << ','
          << r.physicsRate << ',' << r.blockSize << ',' << r.budgetUs << ',' << r.meanUs << ','
          << r.p99Us << ',' << r.maxUs << ',' << 100.0 * r.meanUs / r.budgetUs << ','
          << 100.0 * r.p99Us / r.budgetUs << ',' << 100.0 * r.maxUs / r.budgetUs << ','
          << (r.stable ? "yes" : "NO") << '\n';
}

} // namespace

int main (int argc, char** argv)
{
    Options o;
    if (! parseArgs (argc, argv, o))
        return 1;

   #if ! defined (NDEBUG)
    std::cout << "WARNING: this is a debug build — timings are meaningless. Build with --config Release.\n\n";
   #endif

    std::vector<EngineInfo> engines;
    for (auto& e : allEngines())
        if (o.engines.empty() || std::find (o.engines.begin(), o.engines.end(), e.name) != o.engines.end())
            engines.push_back (e);

    if (engines.empty())
    {
        std::cerr << "no matching engines (use --list)\n";
        return 1;
    }

    std::printf ("Slink benchmark  —  %s  —  sample rate %.0f Hz, %.1f s per config, label \"%s\"\n",
                 juce::SystemStats::getCpuModel().toRawUTF8(), o.sampleRate, o.seconds, o.label.c_str());
    std::printf ("Load = callback time as %% of the real-time budget (block / sample rate). Keep max well under 100%%.\n\n");
    std::printf ("%-16s %10s %6s %10s %10s %10s %10s %9s %9s %9s %7s\n",
                 "engine", "physics Hz", "block", "budget us", "mean us", "p99 us", "max us", "mean", "p99", "max", "stable");

    std::vector<BenchResult> results;

    for (auto& e : engines)
        for (auto rate : o.physicsRates)
            for (auto block : o.blockSizes)
            {
                auto r = runOne (e, o, rate, block);
                results.push_back (r);
                std::printf ("%-16s %10.0f %6d %10.1f %10.2f %10.2f %10.2f %s %s %s %7s\n",
                             r.engine.c_str(), r.physicsRate, r.blockSize, r.budgetUs, r.meanUs, r.p99Us, r.maxUs,
                             pct (r.meanUs, r.budgetUs).c_str(), pct (r.p99Us, r.budgetUs).c_str(),
                             pct (r.maxUs, r.budgetUs).c_str(), r.stable ? "yes" : "NO");
                std::fflush (stdout);
            }

    if (o.writeCsv)
    {
        appendCsv (o, results);
        std::printf ("\nAppended %zu rows to %s\n", results.size(), o.csvPath.c_str());
    }

    return 0;
}
