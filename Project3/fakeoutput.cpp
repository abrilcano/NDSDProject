#include <fstream>
#include <iostream>
#include <cmath>


int main(int argc, char* argv[]) {

    // Create files to verify the output

    for(int i = 0; i < 10; i++) {
        std::ofstream file;
        file.open("heat_diffusion" + std::to_string(i) + ".csv");
        if ( i == 0) {
            file << "x,y,temperature\n";
        }
        for( int j = 0; j < 8; j++) {
            for( int k = 0; k < 8; k++) {
                file << j << "," << k << "," << j*k << "\n";
            }
        }
        file.close();
    }

    return 0;
}