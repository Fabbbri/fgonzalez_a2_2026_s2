#include <iostream>
#include <chrono>

struct ThreadState {
    int progress;
    bool done;
};

void execute_quantum(
    ThreadState& state,
    int max_work,
    std::chrono::microseconds quantum,
    int thread_id
) {
    auto start = std::chrono::steady_clock::now();

    while (!state.done) {

        // Trabajo simulado
        state.progress++;

        if (state.progress >= max_work) {
            state.done = true;
            break;
        }

        // ¿Se agotó el quantum?
        auto now = std::chrono::steady_clock::now();

        if (now - start >= quantum) {
            break;
        }
    }
}

int main() {

    const int max_work = 100000;
    const auto quantum = std::chrono::microseconds(100);

    ThreadState t0{0, false};
    ThreadState t1{0, false};

    int current_thread = 0;

    while (!t0.done || !t1.done) {

        if (current_thread == 0 && !t0.done) {
            execute_quantum(t0, max_work, quantum, 0);
        }
        else if (current_thread == 1 && !t1.done) {
            execute_quantum(t1, max_work, quantum, 1);
        }

        std::cout << "T" << current_thread
          << " -> progreso "
          << (current_thread == 0 ? t0.progress : t1.progress)
          << '\n';

        // Round-Robin
        current_thread = 1 - current_thread;
    }
}