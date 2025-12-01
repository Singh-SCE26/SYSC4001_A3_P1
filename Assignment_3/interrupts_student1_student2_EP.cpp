/**
 * @file interrupts.cpp
 * @author Aryan Singh (101270896)
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

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

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        for (auto &process : list_processes) {
            if (process.arrival_time == current_time && process.state == NEW) {
                // Assign memory (template ignores return value, so we do the same)
                assign_memory(process);

                // Initialize runtime fields
                process.state           = READY;
                process.remaining_time  = process.processing_time;
                process.time_to_next_io = (process.io_freq > 0 ? process.io_freq : 0);
                process.finish_time     = 0;
                // priority already set in add_process (e.g., = PID)

                ready_queue.push_back(process);
                job_list.push_back(process);

                // Log NEW -> READY
                execution_status += print_exec_status(
                    current_time, process.PID, NEW, READY
                );
            }
        }

        // 2) Manage the wait queue

        //    This keeps track of how long a process must remain in the WAITING state
        //////////////////////////////////////////////////////////////////////////////
        for (auto it = wait_queue.begin(); it != wait_queue.end(); ) {
            if (it->wake_time <= current_time) {
                PCB p = *it;

                // WAITING -> READY
                states old_state = p.state;      // should be WAITING
                p.state = READY;

                // Reset I/O countdown for next I/O
                if (p.io_freq > 0) {
                    p.time_to_next_io = p.io_freq;
                }

                // synchronize job_list
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
        // 3) Schedule processes from the ready queue

        //    This keeps track of how long a process must remain in the WAITING state
        //////////////////////////////////////////////////////////////////////////////
        for (auto it = wait_queue.begin(); it != wait_queue.end(); ) {
            if (it->wake_time <= current_time) {
                PCB p = *it;

                // WAITING -> READY
                states old_state = p.state;      // should be WAITING
                p.state = READY;

                // Reset I/O down for next I/O
                if (p.io_freq > 0) {
                    p.time_to_next_io = p.io_freq;
                }

                // synchronize job_list
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

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, assign memory and put the process into the ready queue
                assign_memory(process);

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue
        for (auto it = wait_queue.begin(); it != wait_queue.end(); ) {
            if (it->wake_time <= current_time) {
                PCB p = *it;

                states old_state = p.state;     // should be WAITING
                p.state = READY;

                // Reset countdown to the next I/O
                if (p.io_freq > 0) {
                    p.time_to_next_io = p.io_freq;
                }

                // Sync updated PCB into job_list
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

        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        FCFS(ready_queue); //example of FCFS is shown here


        // If CPU is idle and there is something ready, dispatch it
        if (running.state == NOT_ASSIGNED && !ready_queue.empty()) {
            // run_process() takes the process at ready_queue.back()
            int next_pid = ready_queue.back().PID;

            run_process(running, job_list, ready_queue, current_time);

            // Log READY -> RUNNING transition
            execution_status += print_exec_status(
                current_time, next_pid, READY, RUNNING
            );
        }
        /////////////////////////////////////////////////////////////////
            ///////////////////// 4) EXECUTE 1 ms OF CPU TIME /////////////////////
        if (running.state == RUNNING) {

            // Use 1 ms of CPU time
            running.remaining_time--;

            // If the process does I/O, count down to the next I/O request
            if (running.io_freq > 0 && running.time_to_next_io > 0) {
                running.time_to_next_io--;
            }

            // ---- Case 1: process has finished all its CPU time ----
            if (running.remaining_time == 0) {

                states old_state = running.state;   // RUNNING
                running.state = TERMINATED;
                running.finish_time = current_time; // finished at this simulated time

                // Log RUNNING -> TERMINATED
                execution_status += print_exec_status(
                    current_time,           // time of transition
                    running.PID,            // process ID
                    old_state,              // RUNNING
                    TERMINATED              // new state
                );

                // terminate_process:
                //  - sets remaining_time = 0
                //  - sets state = TERMINATED
                //  - frees memory
                //  - syncs PCB in job_list
                terminate_process(running, job_list);

                // CPU is now idle
                idle_CPU(running);
            }

            // ---- Case 2: not finished, but I/O is now due ----
            else if (running.io_freq > 0 && running.time_to_next_io == 0) {

                states old_state = running.state;  // RUNNING
                running.state = WAITING;

                // I/O starts now and lasts io_duration ms,
                // so process will be ready again at (current_time + io_duration)
                running.wake_time = current_time + running.io_duration;

                // Log RUNNING -> WAITING
                execution_status += print_exec_status(
                    current_time,
                    running.PID,
                    old_state,   // RUNNING
                    WAITING
                );

                // Sync updated PCB (WAITING + wake_time) into job_list
                sync_queue(job_list, running);

                // Move process to wait_queue
                wait_queue.push_back(running);

                // CPU becomes idle again (no process is running)
                idle_CPU(running);
            }

            // ---- Case 3: still running, no finish and no I/O yet ----
            else {
                // Just sync updated remaining_time/time_to_next_io back to job_list
                sync_queue(job_list, running);
            }
        }

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
    //To do so, the add_process() helper function is used (see include file).
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