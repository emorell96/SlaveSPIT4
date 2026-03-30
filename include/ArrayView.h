#include "Arduino.h"
#include <vector>
namespace SlaveSpi
{
    /// @brief A small struct to pass around the data of an array without having to copy it around. This is used to pass the payload of a message.
    /// @tparam T the type of the data in the array, in our case this will be uint16_t since the payload of a message is an array of uint16_t
    /// This is used here because C++17 doesn't have std::span, so this will do for now. It is a simple struct that contains a pointer to the data and the size of the array, and some helper functions to access the data.
    template <typename T>
    struct ArrayView
    {
        /* data */
        T* data = nullptr;
        std::size_t size = 0;

        ArrayView(T*data, std::size_t size) : data(data), size(size) {}
        ArrayView(const std::vector<T>& vec) : data(vec.data()), size(vec.size()) {}

        T& operator[](std::size_t i) const { return data[i]; }
        T* begin() const { return data; }
        T* end() const { return data + size; }
        bool empty() const { return size == 0; }
    };
    
}