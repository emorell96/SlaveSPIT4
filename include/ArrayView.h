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
    private:
        T* data = nullptr;
        const std::size_t _size = 0;
    public:
        ArrayView(T*data, const std::size_t size) : data(data), _size(size) {}
        ArrayView(std::vector<T>& vec) : data(vec.data()), _size(vec.size()) {}

        T& operator[](std::size_t i) const { return data[i]; }
        T* begin() const { return data; }
        T* end() const { return data + _size; }
        bool empty() const { return _size == 0; }
        const std::size_t size() const { return _size; }
    };
    
}