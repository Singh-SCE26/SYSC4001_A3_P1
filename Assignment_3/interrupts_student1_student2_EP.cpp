/**
 * @file interrupts.cpp
 * @author Aryan Singh (101270896)
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include <tuple>
#include"interrupts_student1_student2.hpp"

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time > second.arrival_time); 
                } 
            );
}
std::tuple<std::string /* add std::string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   // READY processes
    std::vector<PCB> wait_queue;    // WAITING (doing I/O)
    std::vector<PCB> job_list;      // all processes that have entered the system

    unsigned int current_time = 0;
    PCB running;

 // Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    // Make the output table (header row)
    execution_status = print_exec_header();

    // Main simulation loop
    while (!all_process_terminated(job_list) || job_list.empty()) {

        ////////////////////////////////////////////////////////////////////////////
        // 1) Admit processes whose arrival_time <= current_time and are NOT_ASSIGNED
        ////////////////////////////////////////////////////////////////////////////
        for (auto &process : list_processes) {
            if (process.arrival_time <= current_time &&
                process.state == NOT_ASSIGNED) {

                // Try to assign memory (fixed partitions)
                if (!assign_memory(process)) {
                    // No space yet; try again in a future tick
                    continue;
                }

                // Initialize runtime fields
                process.state           = READY;
                process.remaining_time  = process.processing_time;
                process.time_to_next_io = (process.io_freq > 0 ? process.io_freq : 0);
                process.finish_time     = 0;

                // Add to ready queue and to job_list
                ready_queue.push_back(process);
                job_list.push_back(process);

                // Log NEW -> READY (even though state was NOT_ASSIGNED internally)
                execution_status += print_exec_status(
                    current_time, process.PID, NEW, READY
                );
            }
        }

        ////////////////////////////////////////////////////////////////////////////
        // 2) Manage WAIT QUEUE (I/O completion)
        ////////////////////////////////////////////////////////////////////////////
        for (auto it = wait_queue.begin(); it != wait_queue.end(); ) {
            if (it->wake_time <= current_time) {
                PCB p = *it;

                states old_state = p.state;   // should be WAITING
                p.state = READY;

                if (p.io_freq > 0) {
                    p.time_to_next_io = p.io_freq;   // reset countdown to next I/O
                }

                // Sync into job_list
                sync_queue(job_list, p);

                execution_status += print_exec_status(
                    current_time, p.PID, old_state, READY
                );

                ready_queue.push_back(p);
                it = wait_queue.erase(it);
            } else {
                ++it;
            }
        }

        ////////////////////////////////////////////////////////////////////////////
        // 3) SCHEDULER: External Priority (NO preemption)
        //    - Higher priority = smaller PID
        //    - Only dispatch when CPU is idle
        ////////////////////////////////////////////////////////////////////////////
        if (running.state == NOT_ASSIGNED && !ready_queue.empty()) {

            // Pick highest priority (lowest PID) from ready_queue
            int best_index = 0;
            for (int i = 1; i < static_cast<int>(ready_queue.size()); ++i) {
                if (ready_queue[i].PID < ready_queue[best_index].PID) {
                    best_index = i;
                }
            }

            // Put chosen process at back (run_process() takes back())
            std::swap(ready_queue[best_index], ready_queue.back());
            int next_pid = ready_queue.back().PID;

            // READY -> RUNNING
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(
                current_time, next_pid, READY, RUNNING
            );
        }

        ////////////////////////////////////////////////////////////////////////////
        // 4) Execute 1 ms of CPU time for the running process
        ////////////////////////////////////////////////////////////////////////////
        if (running.state == RUNNING) {

            // Use 1 ms of CPU
            running.remaining_time--;

            // Count down to the next I/O (if the process does I/O)
            if (running.io_freq > 0 && running.time_to_next_io > 0) {
                running.time_to_next_io--;
            }

            // a) Finished all CPU time -> TERMINATED
            if (running.remaining_time == 0) {

                states old_state = running.state;   // RUNNING
                running.state = TERMINATED;
                running.finish_time = current_time;

                execution_status += print_exec_status(
                    current_time, running.PID, old_state, TERMINATED
                );

                // terminate_process frees memory and syncs to job_list
                terminate_process(running, job_list);
                idle_CPU(running);
            }

            // b) Not finished, but I/O is due -> WAITING
            else if (running.io_freq > 0 && running.time_to_next_io == 0) {

                states old_state = running.state;   // RUNNING
                running.state = WAITING;

                // I/O finishes at (current_time + io_duration)
                running.wake_time = current_time + running.io_duration;

                execution_status += print_exec_status(
                    current_time, running.PID, old_state, WAITING
                );

                // Sync and move to wait_queue
                sync_queue(job_list, running);
                wait_queue.push_back(running);

                idle_CPU(running);
            }

            // c) Still RUNNING, no finish and no I/O yet
            else {
                // Just sync updated remaining_time/time_to_next_io into job_list
                sync_queue(job_list, running);
            }
        }

    
        current_time++;
    }

    // Close the output table
    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}
int main(int argc, char** argv) {

    //Get the input file from the user
    if (argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./EP <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    std::string line;
    std::vector<PCB> list_process;
    while (std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process  = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}
