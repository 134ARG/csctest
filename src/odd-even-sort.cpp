#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <odd-even-sort.hpp>
#include <bits/c++config.h>
#include <mpi.h>
#include <iostream>
#include <vector>

#define DATA_SIZE_TAG 5
#define SEND_BACK_TAG 4
#define SIZE_TAG 3
#define INIT_DATA_TAG 2
#define ODD_SEND 1
#define EVEN_SEND 0;

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
            if (0 == rank) {
 
                size_t data_size = end - begin;
                size_t average = data_size / information->num_of_proc;
                size_t extra = data_size % information->num_of_proc;

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
                        size_t step = average + ((i < extra) ? 1 : 0);
                        MPI_Isend(&step, 1, MPI_UNSIGNED_LONG, i, SIZE_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Isend(&data_size, 1, MPI_UNSIGNED_LONG, i, DATA_SIZE_TAG, MPI_COMM_WORLD, &placeholder);
                        MPI_Isend(&(*start), step, MPI_LONG, i, INIT_DATA_TAG, MPI_COMM_WORLD, &placeholder);
                        start += step;
                    } else {
                        size_t step = 0;
                        MPI_Isend(&step, 1, MPI_UNSIGNED_LONG, i, SIZE_TAG, MPI_COMM_WORLD, &placeholder);
                    }
                }
                
                MPI_Request_free(&placeholder);
            }

            size_t size = 0;
            size_t data_size = 0;
            std::vector<int64_t> elements{};

            MPI_Recv(&size, 1, MPI_UNSIGNED_LONG, 0, SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (size == 0) {
                std::cout << "Not used! I am " << rank << std::endl;
                break;
            }

            elements.resize(size);

            MPI_Recv(&data_size, 1, MPI_UNSIGNED_LONG, 0, DATA_SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            MPI_Recv(&(elements[0]), size, MPI_LONG, 0, INIT_DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << "rank " << rank << " received data." << std::endl;

            auto head = elements.begin();
            auto tail = elements.end() - 1;
                
            int64_t recv_tail = INT64_MAX;
            int64_t send_tail = 0;
            if (rank & 1) {
                int times = data_size;
                MPI_Request placeholder;
                MPI_Request placeholder2;
                while(times--) {
                    MPI_Isend(&(*head), 1, MPI_LONG, rank - 1, 0, MPI_COMM_WORLD, &placeholder);
                    MPI_Recv(&(*head), 1, MPI_LONG, rank - 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    
                    // std::sort(elements.begin(), elements.end());

                    MPI_Recv(&recv_tail, 1, MPI_LONG, rank + 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (recv_tail > *tail) {
                        send_tail = recv_tail;
                    } else {
                        send_tail = *tail;
                        *tail = recv_tail;
                    }
                    MPI_Isend(&send_tail, 1, MPI_LONG, rank + 1, 0, MPI_COMM_WORLD, &placeholder2);
                }
                MPI_Request_free(&placeholder);
                MPI_Request_free(&placeholder2);
            } else {
                int times = data_size;
                MPI_Request placeholder;
                MPI_Request placeholder2;
                while(times--) {
                    MPI_Recv(&recv_tail, 1, MPI_LONG, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (recv_tail > *tail) {
                        send_tail = recv_tail;
                    } else {
                        send_tail = *tail;
                        *tail = recv_tail;
                    }
                    MPI_Isend(&send_tail, 1, MPI_LONG, rank + 1, 1, MPI_COMM_WORLD, &placeholder);
                    
                    // std::sort(elements.begin(), elements.end());

                    MPI_Isend(&(*head), 1, MPI_LONG, rank - 1, 1, MPI_COMM_WORLD, &placeholder2);
                    if (rank != 0) {
                        MPI_Recv(&(*head), 1, MPI_LONG, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                }
                MPI_Request_free(&placeholder);
                MPI_Request_free(&placeholder2);
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
