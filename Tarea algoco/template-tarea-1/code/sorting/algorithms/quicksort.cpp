//fuente: https://www.geeksforgeeks.org/dsa/quicksort-using-random-pivoting/
//este programa deja los comentarios en inglés que están en el original
//pero incluye las librerias de 'vector' y 'utility'
//además de la firma de 'quickSortArray'

// C++ implementation QuickSort
// using Lomuto's partition Scheme.
#include <cstdlib>
#include <vector>
#include <utility>
#include <time.h>
#include <iostream>

using namespace std;

// This function takes last element
// as pivot, places
// the pivot element at its correct
// position in sorted array, and
// places all smaller (smaller than pivot)
// to left of pivot and all greater
// elements to right of pivot
int partition(int arr[], int low, int high)
{
    // pivot
    int pivot = arr[high];

    // Index of smaller element
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++)
    {
        // If current element is smaller
        // than or equal to pivot
        if (arr[j] <= pivot) {

            // increment index of
            // smaller element
            i++;
            swap(arr[i], arr[j]);
        }
    }
    swap(arr[i + 1], arr[high]);
    return (i + 1);
}

// Generates Random Pivot, swaps pivot with
// end element and calls the partition function
int partition_r(int arr[], int low, int high)
{
    static bool seeded = false;
    if (!seeded) {
        srand(static_cast<unsigned>(time(NULL)));
        seeded = true;
    }

    int random = low + rand() % (high - low + 1);

    // Swap A[random] with A[high]
    swap(arr[random], arr[high]);

    return partition(arr, low, high);
}

/* The main function that implements
QuickSort
arr[] --> Array to be sorted,
low --> Starting index,
high --> Ending index */
void quickSort(int arr[], int low, int high)
{
    if (low < high) {

        /* pi is partitioning index,
        arr[p] is now
        at right place */
        int pi = partition_r(arr, low, high);
         // Recurse on the smaller side first to limit recursion depth
        if (pi - low < high - pi) {
            quickSort(arr, low, pi - 1);
            low = pi + 1;
        } else {
            quickSort(arr, pi + 1, high);
            high = pi - 1;
        }
        
    }
}

/* Function to print an array */
void printArray(int arr[], int size)
{
    int i;
    for (i = 0; i < size; i++)
        cout << arr[i] << " ";
}

std::vector<int> quickSortArray(std::vector<int>& arr) {
    if (!arr.empty()) {
        quickSort(arr.data(), 0, static_cast<int>(arr.size()) - 1);
    }
    return arr;
}