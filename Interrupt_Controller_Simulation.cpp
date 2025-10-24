/*
Interrupt Controller Simulation
Language: C++ (C++17)

Features:
- Simulates three I/O devices: Keyboard (high), Mouse (medium), Printer (low)
- Each device runs a thread that periodically generates interrupts (randomized delays)
- Central Interrupt Controller serves highest-priority pending interrupt that is not masked
- Supports runtime masking/unmasking of devices through simple console commands
- Prints clear messages for ISR handling and masked interrupts
- Optional logging: records ISR start time and completion time to "isr_log.txt"

Compile:
    g++ -std=c++17 -pthread Interrupt_Controller_Simulation.cpp -o interrupt_sim
Run:
    ./interrupt_sim

Console commands (type while program is running):
    mask k|m|p      -- mask Keyboard/Mouse/Printer
    unmask k|m|p    -- unmask device
    status          -- show masked/unmasked and pending counts
    exit            -- stop simulation and exit cleanly

Note: This is a simulation for educational purposes (ISR work is represented by delays).
*/

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <climits>
#include <ctime>


#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace std;

enum Device { PRINTER = 1, MOUSE = 2, KEYBOARD = 3 };

struct InterruptEvent {
    Device dev;
    long long seq; // sequence number to break ties (older first)
    chrono::steady_clock::time_point timestamp;
};

// shared state
mutex mtx;
condition_variable cv;
vector<InterruptEvent> pending; // small list; we'll search for highest-priority unmasked
atomic<bool> running{true};

bool masked_keyboard = false;
bool masked_mouse = false;
bool masked_printer = false;

long long global_seq = 0;

// logging
mutex log_mtx;
string log_filename = "isr_log.txt";

void append_log(const string &line) {
    lock_guard<mutex> lg(log_mtx);
    ofstream f(log_filename, ios::app);
    if(f) {
        f << line << "\n";
    }
}

string device_name(Device d) {
    switch(d) {
        case KEYBOARD: return "Keyboard";
        case MOUSE: return "Mouse";
        case PRINTER: return "Printer";
    }
    return "Unknown";
}

// Device thread function: generate interrupts periodically (randomized)
void device_thread(Device dev, int min_ms, int max_ms) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(min_ms, max_ms);

    while (running) {
        int wait_ms = dist(gen);
        this_thread::sleep_for(chrono::milliseconds(wait_ms));
        if(!running) break;

        // push interrupt
        {
            lock_guard<mutex> lg(mtx);
            pending.push_back({dev, ++global_seq, chrono::steady_clock::now()});
        }
        cv.notify_one();
    }
}

// Interrupt Controller: pick highest-priority unmasked interrupt and run its ISR
void controller_thread() {
    while (running) {
        unique_lock<mutex> ul(mtx);
        cv.wait(ul, []{ return !pending.empty() || !running; });
        if(!running && pending.empty()) break;

        // find highest-priority pending event that is not masked
        int best_idx = -1;
        Device best_dev = PRINTER;
        long long best_seq = LLONG_MAX;
        for (int i = 0; i < (int)pending.size(); ++i) {
            Device d = pending[i].dev;
            bool is_masked = (d == KEYBOARD && masked_keyboard) ||
                             (d == MOUSE && masked_mouse) ||
                             (d == PRINTER && masked_printer);
            if (is_masked) continue;
            // priority: KEYBOARD (3) > MOUSE (2) > PRINTER (1)
            if (best_idx == -1 || pending[i].dev > best_dev || (pending[i].dev == best_dev && pending[i].seq < best_seq)) {
                best_idx = i;
                best_dev = pending[i].dev;
                best_seq = pending[i].seq;
            }
        }

        if (best_idx == -1) {
            // all pending are masked - print ignored messages and just wait until masks change or new interrupts
            // To avoid busy waiting, wait on cv until masks change or new unmasked interrupt arrives.
            // But we'll also print masked status for visibility.
            for (auto &ev : pending) {
                Device d = ev.dev;
                bool is_masked = (d == KEYBOARD && masked_keyboard) ||
                                 (d == MOUSE && masked_mouse) ||
                                 (d == PRINTER && masked_printer);
                if (is_masked) {
                    cout << device_name(d) << " Interrupt Ignored (Masked)" << endl;
                }
            }
            // wait for mask change or new events
            cv.wait_for(ul, chrono::milliseconds(200));
            continue;
        }

        // extract event
        InterruptEvent ev = pending[best_idx];
        pending.erase(pending.begin() + best_idx);
        ul.unlock();

        // handle ISR
        auto now = chrono::system_clock::now();
        time_t start_time = chrono::system_clock::to_time_t(now);
        cout << device_name(ev.dev) << " Interrupt Triggered → Handling ISR → ";
        cout << "Started at " << put_time(localtime(&start_time), "%F %T") << endl;

        // log start
        {
            stringstream ss;
            ss << "START | " << device_name(ev.dev) << " | seq=" << ev.seq << " | "
               << put_time(localtime(&start_time), "%F %T");
            append_log(ss.str());
        }

        // Simulate ISR work (vary by device)
        if (ev.dev == KEYBOARD) this_thread::sleep_for(chrono::milliseconds(300));
        else if (ev.dev == MOUSE) this_thread::sleep_for(chrono::milliseconds(500));
        else this_thread::sleep_for(chrono::milliseconds(800));

        auto done = chrono::system_clock::now();
        time_t done_time = chrono::system_clock::to_time_t(done);
        cout << device_name(ev.dev) << " ISR Completed at " << put_time(localtime(&done_time), "%F %T") << endl;

        // log completion
        {
            stringstream ss;
            ss << "END   | " << device_name(ev.dev) << " | seq=" << ev.seq << " | "
               << put_time(localtime(&done_time), "%F %T");
            append_log(ss.str());
        }
    }
}

void user_input_thread() {
    string cmd;
    while (running) {
        if(!getline(cin, cmd)) break; // e.g., EOF
        if (cmd.empty()) continue;
        stringstream ss(cmd);
        string token;
        ss >> token;
        if (token == "mask") {
            string which; ss >> which;
            if (which == "k") { masked_keyboard = true; cout << "Keyboard masked." << endl; }
            else if (which == "m") { masked_mouse = true; cout << "Mouse masked." << endl; }
            else if (which == "p") { masked_printer = true; cout << "Printer masked." << endl; }
            else cout << "Unknown device. Use k/m/p." << endl;
            cv.notify_one();
        } else if (token == "unmask") {
            string which; ss >> which;
            if (which == "k") { masked_keyboard = false; cout << "Keyboard unmasked." << endl; }
            else if (which == "m") { masked_mouse = false; cout << "Mouse unmasked." << endl; }
            else if (which == "p") { masked_printer = false; cout << "Printer unmasked." << endl; }
            else cout << "Unknown device. Use k/m/p." << endl;
            cv.notify_one();
        } else if (token == "status") {
            lock_guard<mutex> lg(mtx);
            cout << "Status:\n";
            cout << "  Keyboard: " << (masked_keyboard?"Masked":"Unmasked") << "\n";
            cout << "  Mouse:    " << (masked_mouse?"Masked":"Unmasked") << "\n";
            cout << "  Printer:  " << (masked_printer?"Masked":"Unmasked") << "\n";
            cout << "  Pending interrupts: " << pending.size() << "\n";
        } else if (token == "exit") {
            cout << "Exiting..." << endl;
            running = false;
            cv.notify_all();
            break;
        } else {
            cout << "Commands: mask k|m|p, unmask k|m|p, status, exit" << endl;
        }
    }
}

int main() {
    // clear log file
    {
        lock_guard<mutex> lg(log_mtx);
        ofstream f(log_filename, ios::trunc);
        if (f) f << "ISR Log Started: " << chrono::system_clock::to_time_t(chrono::system_clock::now()) << "\n";
    }

    cout << "Interrupt Controller Simulation (type 'status' to see masks and pending interrupts)" << endl;
    cout << "Commands: mask k|m|p, unmask k|m|p, status, exit" << endl;

    thread t_keyboard(device_thread, KEYBOARD, 800, 2000); // generate every 0.8-2s
    thread t_mouse(device_thread, MOUSE, 1000, 3000);     // 1-3s
    thread t_printer(device_thread, PRINTER, 1500, 4000); // 1.5-4s

    thread t_controller(controller_thread);
    thread t_user(user_input_thread);

    // join
    t_user.join(); // user types exit
    // ensure running==false
    running = false;
    cv.notify_all();

    t_keyboard.join();
    t_mouse.join();
    t_printer.join();
    t_controller.join();

    cout << "Simulation terminated. Log saved to " << log_filename << endl;
    return 0;
}
