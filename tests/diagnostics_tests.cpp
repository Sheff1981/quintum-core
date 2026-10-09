#include "net/diagnostics.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
using quintum::net::Diagnostics;
int main(){
 auto dir=std::filesystem::temp_directory_path()/("quintum-diagnostics-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(dir);
 std::ofstream(dir/"wallet.dat")<<"wallet sentinel";
 std::ofstream(dir/"chain.dat")<<"chain sentinel";
 {
 Diagnostics d(dir);
 assert(d.snapshot_json().find("\"local_height\":null")!=std::string::npos);
 assert(d.snapshot_json().find("\"network_progress_seen\":false")!=std::string::npos);
 d.stage("header_wait");
 const auto initial_log = d.export_log();
 for (int repeat = 0; repeat < 20; ++repeat) d.stage("header_wait");
 assert(d.export_log() == initial_log);
 std::this_thread::sleep_for(std::chrono::milliseconds(5));
 d.stage("header_validation");
 const auto transition_log = d.export_log();
 const auto completed = transition_log.rfind("\"event\":\"stage_complete\"");
 assert(completed != std::string::npos);
 const auto duration_position = transition_log.find("\"duration_ms\":", completed);
 assert(duration_position != std::string::npos);
 assert(std::stoull(transition_log.substr(duration_position + 14)) >= 5);
 assert(transition_log.find("\"stage\":\"header_wait\"", completed) != std::string::npos);
 d.network_progress();
 assert(d.snapshot_json().find("\"network_progress_seen\":true") != std::string::npos);
 d.attempt("endpoint\"\\\n");d.attempt("peer");d.peer("endpoint\"\\",7,42);d.local_height(12);
 d.failure("sync",3,"fixed \"description\"\\");d.counter("blocks_accepted",5);d.gauge("header_batch_size",2000);d.duration("randomx_verify_ms",123000);
 auto s=d.snapshot_json();assert(s.find("\"retry_count\":1")!=std::string::npos);assert(s.find("\"header_batch_size\":2000")!=std::string::npos);assert(s.find("\"randomx_verify_ms\":123")!=std::string::npos);assert(s.find("endpoint\\\"\\\\")!=std::string::npos);
 d.peer("", 7, 42);
 assert(d.snapshot_json().find("endpoint\\\"\\\\") != std::string::npos);
 d.event("sync", "info", "performance");
 const auto performance = d.export_log();
 assert(performance.find("\"randomx_verify_ms\":123") != std::string::npos);
 assert(performance.find("\"header_batch_size\":2000") != std::string::npos);
 d.synchronized();
 const auto synchronized_log = d.export_log();
 d.synchronized();
 assert(d.export_log() == synchronized_log);
 assert(d.snapshot_json().find("\"last_sync_utc\":\"20")!=std::string::npos);
 for(int i=0;i<3000;++i)d.event("sync","info","metadata");
 assert(std::filesystem::file_size(dir/"diagnostics.jsonl")<=256*1024);
 assert(std::filesystem::file_size(dir/"diagnostics.previous.jsonl")<=256*1024);
 auto log=d.export_log();assert(!log.empty()&&log.size()<=512*1024&&log.back()=='\n');
 std::vector<std::thread> workers;for(int i=0;i<4;++i)workers.emplace_back([&]{for(int j=0;j<300;++j){d.counter("blocks_received",1);d.stage("header_validation");assert(!d.snapshot_json().empty());}});for(auto& t:workers)t.join();
 assert(d.snapshot_json().find("\"blocks_received\":1200")!=std::string::npos);
 }
 {
 Diagnostics d(dir);auto s=d.snapshot_json();assert(s.find("\"last_sync_utc\":\"20")!=std::string::npos);assert(s.find("\"stage\":\"idle\"")!=std::string::npos);
 d.counter("blocks_received",9);assert(d.clear_log());assert(d.export_log().empty());assert(d.snapshot_json().find("\"blocks_received\":9")!=std::string::npos);
 assert(std::filesystem::exists(dir/"wallet.dat")&&std::filesystem::exists(dir/"chain.dat"));
 // A directory in place of the journal gives a deterministic write error even as root.
 std::filesystem::create_directory(dir/"diagnostics.jsonl");d.event("sync","info","test");assert(d.snapshot_json().find("diagnostic journal write failed")!=std::string::npos);d.export_log();assert(d.snapshot_json().find("diagnostic export read failed")!=std::string::npos);
 }
 std::filesystem::remove_all(dir);
}
