#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <odd-even-sort.hpp>
#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <vector>

#define USED_PROC_TAG 6
#define DATA_SIZE_TAG 5
#define SEND_BACK_TAG 4
#define SIZE_TAG 3
#define INIT_DATA_TAG 2
#define EVEN_SEND 1
#define ODD_SEND 0

namespace sort {
    using namespace std::chrono;


    Context::Context(int &argc, char **&argv) : argc(argc), argv(argv) {
        MPI_Init(&argc, &argv);
    }

    Context::~Context() {
        MPI_Finalize();
    }

    std::unique_ptr<Information> Context::mpi_sort(Element *begin, Element *end) const {
        int res;
        int rank;
        std::unique_ptr<Information> information{};

        res = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (MPI_SUCCESS != res) {
            throw std::runtime_error("failed to get MPI world rank");
        }
        

        if (0 == rank) {
            information = std::make_unique<Information>();
            information->length = end - begin;
            res = MPI_Comm_size(MPI_COMM_WORLD, &information->num_of_proc);
            if (MPI_SUCCESS != res) {
                throw std::runtime_error("failed to get MPI world size");
            };
            information->argc = argc;
            for (auto i = 0; i < argc; ++i) {
                information->argv.push_back(argv[i]);
            }
            information->start = high_resolution_clock::now();
        }
        
        std::vector<int> sendcounts{};
        std::vector<int> displs{};

        do {
            /// now starts the main sorting procedure
            /// @todo: please modify the following code
            unsigned long data_size;
            unsigned long average;
            unsigned long extra;
            unsigned long used;

            if (0 == rank) {
 
                data_size = end - begin;
                average = data_size / information->num_of_proc;
                extra = data_size % information->num_of_proc;
                used = 0;
                if (average > 0) {
                    used = information->num_of_proc;
                } else {
                    used = extra;
                }

                sendcounts = std::vector<int> (information->num_of_proc, average);
                displs = std::vector<int> (information->num_of_proc, 0);

                size_t index = extra;
                while (index != 0) {
                    sendcounts[index-1]++;
                    index--;
                }
                for (size_t i = 1; i < information->num_of_proc; i++) {
                    displs[i] = displs[i - 1] + sendcounts[i - 1];
                }

                MPI_Request placeholder;
                auto start = begin;
                for (size_t i = 0; i < information->num_of_proc; i++) {
                    if (start < end) {
                        unsigned long step = sendcounts[i];
                        std::cout <<  "send process limit " << used << " to process " << i << std::endl;
                        std::cout << "send step " << step << " to process " << i << std::endl;
                        MPI_Isend(&(sendcounts[i]), 1, MPI_INT, i, SIZE_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Request_free(&placeholder);
                        MPI_Isend(&used, 1, MPI_UNSIGNED_LONG, i, USED_PROC_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Request_free(&placeholder);
                        MPI_Isend(&data_size, 1, MPI_UNSIGNED_LONG, i, DATA_SIZE_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Request_free(&placeholder);
                        MPI_Isend(&(*start), step, MPI_LONG, i, INIT_DATA_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Request_free(&placeholder);
                        start += step;
                    } else {
                        int step = 0;
                        std::cout << "send step " << step << " to process " << i << std::endl;
                        MPI_Send(&step, 1, MPI_INT, i, SIZE_TAG, MPI_COMM_WORLD);
                    }
                }
                
                // MPI_Request_free(&placeholder);
            }

            int size = 0;
            data_size = 0;
            unsigned long process_limit = 0;
            std::vector<int64_t> elements{};

            
            MPI_Recv(&size, 1, MPI_INT, 0, SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << "rank " << rank << " size " << size << std::endl;
            if (size == 0) {
                std::cout << "Not used! I am " << rank << std::endl;
                break;
            }

            elements.resize(size);

            MPI_Recv(&process_limit, 1, MPI_UNSIGNED_LONG, 0, USED_PROC_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << process_limit << " rank " << rank << std::endl;

            MPI_Recv(&data_size, 1, MPI_UNSIGNED_LONG, 0, DATA_SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << "data size is " << data_size << std::endl;
            MPI_Recv(&(elements[0]), size, MPI_LONG, 0, INIT_DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << "rank " << rank << " received data." << std::endl;


            auto head = elements.begin();
            auto tail = elements.end() - 1;
                
            int64_t recv_tail = INT64_MAX;
            int64_t send_tail = 0;
 
            int send_tag = -1;
            int receive_tag = -1;

            if (rank & 1) {
                send_tag = ODD_SEND;
                receive_tag  = EVEN_SEND;
            } else {
                send_tag = EVEN_SEND;
                receive_tag  = ODD_SEND;
            }

            int times = data_size/2+1;
            MPI_Request send_head_request;
            MPI_Request recv_head_request;
            MPI_Request send_tail_request;

            while(times--) {
                // internel odd pass
                for (auto i = elements.begin()+1; i < elements.end(); i+=2) {
                    if (*i < *(i-1)) {
                        std::swap(*i, *(i-1));
                    }
                }

                // internel even pass
                for (auto i = elements.begin()+2; i < elements.end(); i += 2) {
                    if (*i < *(i-1)) {
                        std::swap(*i, *(i-1));
                    }
                }
                if (rank > 0) {
                    MPI_Isend(&(*head), 1, MPI_LONG, rank - 1, send_tag, MPI_COMM_WORLD, &send_head_request);
                    MPI_Request_free(&send_head_request);
                    MPI_Irecv(&(*head), 1, MPI_LONG, rank - 1, receive_tag, MPI_COMM_WORLD, &recv_head_request);
                    MPI_Wait(&recv_head_request, MPI_STATUS_IGNORE);
                }
                if (rank + 1 < process_limit) {
                    MPI_Recv(&recv_tail, 1, MPI_LONG, rank + 1, receive_tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    // std::cout << "recv tail ends: " << rank << std::endl; 
                    send_tail = (recv_tail > *tail) ? recv_tail : *tail;
                    if (recv_tail > *tail) {
                        send_tail = recv_tail;
                    } else {
                        send_tail = *tail;
                        *tail = recv_tail;
                    }
                    MPI_Isend(&send_tail, 1, MPI_LONG, rank + 1, send_tag, MPI_COMM_WORLD, &send_tail_request);
                    MPI_Request_free(&send_tail_request);
                }
            }
            

            MPI_Gatherv(&(elements[0]), size, MPI_LONG, &(*begin), &(sendcounts[0]), &(displs[0]), MPI_LONG, 0, MPI_COMM_WORLD);

        } while(0);

        if (0 == rank) {
            information->end = high_resolution_clock::now();
        }

        return information;
    }

    std::ostream &Context::print_information(const Information &info, std::ostream &output) {
        auto duration = info.end - info.start;
        auto duration_count = duration_cast<nanoseconds>(duration).count();
        auto mem_size = static_cast<double>(info.length) * sizeof(Element) / 1024.0 / 1024.0 / 1024.0;
        output << "input size: " << info.length << std::endl;
        output << "proc number: " << info.num_of_proc << std::endl;
        output << "duration (ns): " << duration_count << std::endl;
        output << "throughput (gb/s): " << mem_size / static_cast<double>(duration_count) * 1'000'000'000.0
               << std::endl;
        return output;
    }
}
