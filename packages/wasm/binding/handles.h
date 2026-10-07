#pragma once

#include <memory>
#include <unordered_map>

// Tiny integer-handle table so JS never holds raw pointers.
template <typename T>
class HandleTable {
 public:
  int insert(std::unique_ptr<T> value) {
    const int id = next_++;
    items_[id] = std::move(value);
    return id;
  }
  T* get(int id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
  }
  void erase(int id) { items_.erase(id); }

 private:
  int next_ = 1;
  std::unordered_map<int, std::unique_ptr<T>> items_;
};
