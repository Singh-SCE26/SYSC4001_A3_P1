/**
 * @file interrupts.cpp
 * @author Aryan Singh 101270896
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include "interrupts_student1_student2.hpp"

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
        ready_queue.begin(),
        ready_queue.end(),
        []( const PCB &first, const PCB &second ){
            return (first.arrival_time > second.arrival_time); 
        } 
    );
}

std::tuple<std::string /* add std::string for bonus mark */ >
run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;        //The ready queue of processes
    std::vector<PCB> wait_queue;         //The wait queue of processes
    std::vector<unsigned int> wait_until; // wake-up times for wait_queue processes
    std::vector<PCB> job_list;           //Track all processes

    unsigned int current_time   = 0;
    const unsigned int time_quantum = 100;
    unsigned int slice_remaining = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        // 1) Populate the ready queue with processes as they arrive
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //assign memory and put the process into the ready queue
                bool ok = assign_memory(process);
                if (!ok) {
                    // no memory yet, process stays effectively in NEW
                    continue;
                }

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process);    //Add it to the list of processes

                execution_status += print_exec_status(
                    current_time, process.PID, NEW, READY
                );
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //Move processes whose I/O completed from WAITING to READY
        for (std::size_t i = 0; i < wait_queue.size(); ) {
            if (wait_until[i] <= current_time) {
                PCB p = wait_queue[i];

                states old_state = p.state; // WAITING
                p.state = READY;

                // sync with job_list
                sync_queue(job_list, p);

                execution_status += print_exec_status(
                    current_time, p.PID, old_state, READY
                );

                // move back to ready_queue
                ready_queue.push_back(p);

                // remove from wait structures
                wait_queue.erase(wait_queue.begin() + i);
                wait_until.erase(wait_until.begin() + i);
            } else {
                ++i;
            }
        }
        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        // External Priorities + 100 ms Round Robin (preemptive)
        // Higher priority = lower PID
        if (running.state == NOT_ASSIGNED && !ready_queue.empty()) {
            // find index of highest-priority (lowest PID)
            int best_index = 0;
            for (int i = 1; i < (int)ready_queue.size(); ++i) {
                if (ready_queue[i].PID < ready_queue[best_index].PID) {
                    best_index = i;
                }
            }

            // put best process at back so run_process() can pick it
            std::swap(ready_queue[best_index], ready_queue.back());
            int next_pid = ready_queue.back().PID;

            // READY -> RUNNING
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(
                current_time, next_pid, READY, RUNNING
            );

            slice_remaining = time_quantum;
        }
        /////////////////////////////////////////////////////////////////

        //////////////////////////CPU EXECUTION//////////////////////////
        // 4) Execute 1 ms of CPU time for the running process
        if (running.state == RUNNING) {

            // use 1 ms of CPU
            running.remaining_time--;

            // total CPU used so far
            unsigned int cpu_used =
                running.processing_time - running.remaining_time;

            // a) finished? -> TERMINATED
            if (running.remaining_time == 0) {

                states old_state = running.state; // RUNNING
                running.state = TERMINATED;

                execution_status += print_exec_status(
                    current_time, running.PID, old_state, TERMINATED
                );

                terminate_process(running, job_list); // frees memory + sync
                idle_CPU(running);
                slice_remaining = 0;
            }

            // b) I/O request due? -> WAITING
            else if (running.io_freq > 0 &&
                     cpu_used > 0 &&
                     cpu_used % running.io_freq == 0) {

                states old_state = running.state; // RUNNING
                running.state = WAITING;

                execution_status += print_exec_status(
                    current_time, running.PID, old_state, WAITING
                );

                // sync and move to wait_queue
                sync_queue(job_list, running);
                wait_queue.push_back(running);
                wait_until.push_back(current_time + running.io_duration);

                idle_CPU(running);
                slice_remaining = 0;
            }

            // c) still RUNNING: check priority preemption / quantum expiry
            else {
                if (slice_remaining > 0) {
                    slice_remaining--;
                }

                // check if any READY process has higher priority (lower PID)
                bool higher_priority_ready = false;
                for (const auto &p : ready_queue) {
                    if (p.PID < running.PID) {
                        higher_priority_ready = true;
                        break;
                    }
                }

                // preempt if higher-priority ready OR quantum expired
                if (higher_priority_ready || slice_remaining == 0) {

                    states old_state = running.state; // RUNNING
                    running.state = READY;

                    execution_status += print_exec_status(
                        current_time, running.PID, old_state, READY
                    );

                    // sync updated PCB
                    sync_queue(job_list, running);

                    // requeue at back (RR within same priority)
                    ready_queue.push_back(running);

                    idle_CPU(running);
                } else {
                    // keep running, just sync updated remaining_time
                    sync_queue(job_list, running);
                }
            }
        }
        /////////////////////////////////////////////////////////////////

        // advance simulated time
        current_time++;
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
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
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}
