#include <odd-even-sort.hpp>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    sort::Context context(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    if (argc < 3) {
        if (rank == 0) {
            std::cerr << "wrong arguments" << std::endl;
            std::cerr << "usage: " << argv[0] << " <input-file> <output-file>" << std::endl;
        }
        return 0;
    }

    if (rank == 0) {
        sort::Element element;
        std::vector<sort::Element> data;
        std::ifstream input(argv[1]);
        std::ofstream output(argv[2]);
        while (input >> element) {
            data.push_back(element);
        }
        auto info = context.mpi_sort(data.data(), data.data() + data.size());
        sort::Context::print_information(*info, std::cout);
        for (auto i : data) {
            output << i << std::endl;
        }
    } else {
        context.mpi_sort(nullptr, nullptr);
    }
}
