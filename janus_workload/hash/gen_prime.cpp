// This program is not expected to run on Gem5
// It is used to find the proper size of hash table

#include <iostream>
#include <cstdlib>

// source: http://stackoverflow.com/questions/4424374/determining-if-a-number-is-prime
bool isPrime(int number){

    if(number < 2) return false;
    if(number == 2) return true;
    if(number % 2 == 0) return false;

    for(int i=3; (i*i) <= number; i+=2){
        if(number % i == 0 ) return false;
    }

    return true;
}

int main(const int argc, const char** argv) {
	if (argc != 2) {
		std::cout << "Usage: ./gen_prime <expected value>, Output: \
						the smallest prime > the expected value" << std::endl;
		return 0;
	}
	int expected_val = atoi(argv[1]);
	while (!isPrime(expected_val)) {
		expected_val++;
	}
	std::cout << "Prime: " << expected_val << std::endl;
}
