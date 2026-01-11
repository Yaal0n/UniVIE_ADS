#ifndef ADS_SET_H
#define ADS_SET_H
/*
ADS_set.h - Samuel Sulovsky
*/
#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

template <typename Key, size_t N = 2> // N = bucketsize
class ADS_set {
public:
  class Iterator;
  using value_type = Key;
  using key_type = Key;
  using reference = key_type &;
  using const_reference = const key_type &;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using key_compare = std::less<key_type>;   // B+-Tree
  using key_equal = std::equal_to<key_type>; // Hashing
  using hasher = std::hash<key_type>;        // Hashing

private:

  // Bucket class to hold data
  class Bucket
  {
    public:
      key_type contents[N]; // Static array for data to be saved in the bucket
      size_type currentBucketSize; // Number of occupied slots in the bucket
      Bucket* overflowBucket {nullptr};
      Bucket() {currentBucketSize = 0;}
      ~Bucket()
      { 
        // Recursively delete overflow Buckets
        if(this->overflowBucket != nullptr) delete this->overflowBucket;
      }
  };
  
  // ADS_set variables
  Bucket** table{nullptr}; // Dynamically allocated array of Buckets representing the data structure
  size_type d {0}; // current depth, used to determine when the table is to be expanded
                   // also used to determine target Bucket for new data and find existing data
  size_type nextToSplit {0}; // next Buccket to be split when necessary
  size_type currentTableSize{0}; // current size of the table representing the data structure (visible)
  size_type allocSize {0}; // current allocated size of the table representing the data structure (invisible)
                           // used for optimisation purposes
  size_type numElements {0}; // number of data items stored in the data structure

  // Hash function
  size_type h(const key_type &key, size_type d) const { return hasher{}(key) % (1ul<<d); }

  // Find functions, forward declarations
  Bucket* find_(const key_type &key) const; // find 'row' in the table
  Bucket* find_ofb(const key_type &key) const; // find 'column' in the table
  // Insert functions, forward declarations
  Bucket* insert_(const key_type &key); // standard insertion function
  Bucket* insert_noexcept(const key_type &key); // insert without raising exceptions (without splitting/rehashing the table)

  // Rehash function without allocation. fast, but allocSize > currentTableSize necessary
  void rehash_noalloc() 
  {
    // Create new Bucket and increase visible size of the table
    table[currentTableSize] = new Bucket;
    ++currentTableSize;
    // Push contents of Bucket to be split to a vector.
    
    Bucket* rehashBucket = table[nextToSplit];
    std::vector<key_type> vals {};

    for(size_type i {0}; i < rehashBucket->currentBucketSize; ++i) 
    {
      vals.push_back(rehashBucket->contents[i]);
    }

    while(rehashBucket->overflowBucket != nullptr)
    {
      rehashBucket = rehashBucket->overflowBucket;
      for(size_type i {0}; i < rehashBucket->currentBucketSize; ++i) 
      {
        vals.push_back(rehashBucket->contents[i]);
      }
    }

    // Delete the original Bucket, replace it with an empty one to be filled
    delete table[nextToSplit];
    table[nextToSplit] = new Bucket;
    ++nextToSplit;

    // Split the values between the new bucket and the original bucket
    for(auto elem : vals)  
    {
      insert_noexcept(elem);
    }
  }

  // Complete rehash function
  // Creates a new table with a greater memory allocation and copies all elements over
  // from the old table, then deletes it.
  void rehash()
  {
    size_type newAllocSize {static_cast<size_type>(allocSize*1.3) + 1};
    Bucket** newTable {new Bucket*[newAllocSize]};
    size_type oldTableSize {currentTableSize};
    Bucket** oldTable {table};
    table = newTable;
    ++currentTableSize;
    allocSize = newAllocSize;

    // Corner case - table has size of 0 (no memory allocated so far)
    if(!oldTableSize) 
    {
      table[0] = new Bucket;
      return;
    }

    // place unaffected elements into the necessary buckets
    for (size_type index {0}; index < oldTableSize; ++index)
    {
      if(index == nextToSplit) 
      {
        table[nextToSplit] = new Bucket;
        continue;
      }
      newTable[index] = oldTable[index];
    }
    table[oldTableSize] = new Bucket;
    Bucket* currentBucket = oldTable[nextToSplit];
     ++nextToSplit;

    // rehash the split 'row' in the table
    for(size_type i {0}; i < currentBucket->currentBucketSize; ++i) 
    { 
        insert_noexcept(currentBucket->contents[i]);
    }
    
    while (currentBucket->overflowBucket != nullptr)
    {
      currentBucket = currentBucket->overflowBucket;
      for(size_type i {0}; i < currentBucket->currentBucketSize; ++i) 
      {
        insert_noexcept(currentBucket->contents[i]);
      }
    }

    // clean up
    delete oldTable[nextToSplit-1]; 
    delete[] oldTable; 
  } 

public:
  // Constructors & Destructor
  ADS_set(){};
  ADS_set(std::initializer_list<key_type> ilist) : ADS_set() { insert(ilist); }
  template<typename InputIt> ADS_set(InputIt first, InputIt last) : ADS_set() {insert(first, last); }
  ADS_set(const ADS_set &other){ operator=(other); }
  ~ADS_set()
  {
    clear();
    delete[] table;
  }

  ADS_set &operator=(const ADS_set &other)
  {
    if(this == &other) return *this;

    clear();

    for(auto elem : other)
    {
      insert_(elem);
      ++numElements;
    }

    return *this;
  }

  ADS_set &operator=(std::initializer_list<key_type> ilist)
  {
    clear();
    if(ilist.size() == 0) return *this;
    insert(ilist);
    return *this;
  }

  size_type size() const { return numElements; }
  bool empty() const { return numElements == 0; }

  // count number of occurences of key in the data structure
  size_type count(const key_type &key) const { return !!find_(key); }

  // Returns an iterator to element key if it's present,
  // if it isn't, return end()
  iterator find(const key_type &key) const 
  {
    // if element is not present, there will also be no iterator pointing to it
    // if that is the case, there's no point in looking for it
    if(!count(key)) return end();

    Bucket* destBucket = nullptr;
    int row_Idx = -1;
    int ele_Idx = -1;

    size_type a = h(key, d);
    if (a < nextToSplit) a = h(key, d+1);

    for (size_type i {0}; i < table[a]->currentBucketSize; ++i)
    {
      if(key_equal{}(table[a]->contents[i], key))
      {
        row_Idx = a;
        ele_Idx = i;
        destBucket = table[a];
      }
    }

    Bucket* currentBucket = table[a];

    while (!(currentBucket->overflowBucket == nullptr))
    {
      currentBucket = currentBucket->overflowBucket;
      for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
      {
        if(key_equal{}(currentBucket->contents[i], key))
        {
          row_Idx = a;
          ele_Idx = i;
          destBucket = currentBucket;
        }
      }
    }

    Iterator it = Iterator(&key, this, destBucket, row_Idx, ele_Idx);
    return it;
  }

  // Delete all elements from the table
  void clear()
  {
    if(table == nullptr) return;
  
    for(size_type i {0}; i < currentTableSize; ++i) 
    {
      delete table[i];
    }

    delete[] table;

    table = nullptr;
    numElements = currentTableSize = d = allocSize = nextToSplit = 0;
  }
  
  // Swap contents with other ADS_set
  void swap(ADS_set &other) 
  {
    std::swap(table, other.table);
    std::swap(nextToSplit, other.nextToSplit);
    std::swap(numElements, other.numElements);
    std::swap(allocSize, other.allocSize);
    std::swap(d, other.d);
    std::swap(currentTableSize, other.currentTableSize);
  }

  // insert the contents of an std::intialiser_list<key_type> into the table
  void insert(std::initializer_list<key_type> ilist) 
  {
    if (ilist.size() == 0) return;
    insert(std::begin(ilist), std::end(ilist)); 
  }

  // insert key into the table
  // Returns a pair of iterator and bool,
  // iterator points to the inserted element, or if the element was already present
  // to that element instead
  // bool is true if an element was inserted, false otherwise
  std::pair<iterator,bool> insert(const key_type &key) 
  {
    if(count(key)) return std::make_pair(find(key), false);

    insert_(key);
    ++numElements; 

    return std::make_pair(Iterator(&key, this, find_ofb(key), find_row(key), find_idx(key)), true);
  }

  // inserts the elements between InputIt first and InputIt last into the table
  template<typename InputIt> void insert(InputIt first, InputIt last) 
  {
    for (auto it {first}; it != last; ++it)
    {
      if(count(*it)) continue;
      insert_(*it);
      ++numElements; 
    } 
  }

  // deletes key from the table if it is present
  size_type erase(const key_type &key) 
  {
    if(!count(key)) return 0;

    Bucket* currentBucket = find_ofb(key);
    if(currentBucket == nullptr) return 0;
    bool flagMove {false};

    for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
    {
      if(key_equal{}(currentBucket->contents[i], key)){
        flagMove = true;
        continue;
      }
      if(!flagMove) continue;

      currentBucket->contents[i-1] = currentBucket->contents[i];
    }

    --(currentBucket->currentBucketSize);
    --numElements;
    return 1;
  }

  // Function that returns an iterator to the first element in the table
  // if the table is empty, return the end iterator
  const_iterator begin() const
  {
    if(numElements == 0) return end();
    
    size_type i = 0;

    Bucket* currentBucket = table[i];

    while(currentBucket->currentBucketSize == 0){
        if (currentBucket->overflowBucket == nullptr) //last overflowBucket
        {
          ++i;
          currentBucket = table[i];
        } else
        {
          currentBucket = currentBucket->overflowBucket;
        }            
    }
    Iterator it = Iterator(currentBucket->contents, this, currentBucket, i, 0);
    return it;
  }

  // returns the end iterator
  const_iterator end() const
  {
    return Iterator(this);
  }

  // Dump information about the ADS_set to the specified std::ostream
  void dump(std::ostream &o = std::cerr) const;

  // Help function that finds the position of the key in its corresponding Bucket
  int find_idx(const key_type &key) const 
  {
    if(numElements == 0) return -1; // error

    size_type a = h(key, d);
    if (a < nextToSplit) a = h(key, d+1);

    for (size_type i {0}; i < table[a]->currentBucketSize; ++i)
    {
      if(key_equal{}(table[a]->contents[i], key)) return i;
    }

    Bucket* currentBucket = table[a];

    while (!(currentBucket->overflowBucket == nullptr))
    {
      currentBucket = currentBucket->overflowBucket;
      for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
      {
        if(key_equal{}(currentBucket->contents[i], key)) return i;
      }
  }
  // nothing was found
  return -1;
  }

  ///  Help function that finds the row in which key is located in the table
  int find_row(const key_type &key) const 
  {
    if(numElements == 0) return -1; // error

    size_type a = h(key, d);
    if (a < nextToSplit) a = h(key, d+1);

    for (size_type i {0}; i < table[a]->currentBucketSize; ++i)
    {
      if(key_equal{}(table[a]->contents[i], key)) return a;
    }

    Bucket* currentBucket = table[a];

    while (currentBucket->overflowBucket != nullptr)
    {
      currentBucket = currentBucket->overflowBucket;
      for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
      {
        if(key_equal{}(currentBucket->contents[i], key)) return a;
      }
  }
  // nothing was found
  return -1;
  } 

  // (in)equality operators for ADS_set
  friend bool operator==(const ADS_set &lhs, const ADS_set &rhs)
  {
    if(lhs.size() != rhs.size()) return false;

    for(size_type a {0}; a < lhs.currentTableSize; ++a)
    {
      for (size_type i {0}; i < lhs.table[a]->currentBucketSize; ++i)
      {
        if(!rhs.count(lhs.table[a]->contents[i])) return false;
      }

      Bucket* currentBucket = lhs.table[a];

      while (currentBucket->overflowBucket != nullptr)
      {
        currentBucket = currentBucket->overflowBucket;
        for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
        {
          if(!rhs.count(currentBucket->contents[i])) return false;
        }
      }
    }
    // everything matches
    return true;
  }

  friend bool operator!=(const ADS_set &lhs, const ADS_set &rhs)
  {
    return !(lhs == rhs);
  }

  // (in)equality operators for Bucket
  friend bool operator==(const Bucket &lhs, const Bucket &rhs)
  {
    if (lhs.currentBucketSize != rhs.currentBucketSize) return false;
    if ((lhs.overflowBucket != nullptr) != (rhs.overflowBucket != nullptr)) return false;

    for (size_type i {0}; i < lhs.currentBucketSize; i++)
    {
      if(std::not_equal_to(lhs.contents[i], rhs.contents[i])) return false;
    }

    if(lhs.overflowBucket == nullptr && rhs.overflowBucket == nullptr) return true;

    Bucket* currentBucketLhs = lhs;
    Bucket* currentBucketRhs = rhs;

    while (currentBucketLhs->overflowBucket != nullptr && currentBucketRhs->overflowBucket != nullptr)
    {
      for (size_type i {0}; i < currentBucketLhs->currentBucketSize; i++)
      {
        if(std::not_equal_to(currentBucketLhs->contents[i], currentBucketRhs->contents[i])) return false;
      }

      currentBucketLhs = currentBucketLhs->overflowBucket;
      currentBucketRhs = currentBucketRhs->overflowBucket;
    } 
    
    return true;
  }

  friend bool operator!=(const Bucket &lhs, const Bucket &rhs)
  {
    return !(lhs == rhs);
  }
};

// Forward declared functions

// Help function that inserts a key into the table
// this function does not check whether the key is already present
// it is called by the various insert functions after the necessary
// checks have been made, so no duplicates are inserted
template <typename Key, size_t N>
typename ADS_set<Key,N>::Bucket *ADS_set<Key,N>::insert_(const key_type &key)
{
  size_type a = h(key, d);
  if (a < nextToSplit) a = h(key, d+1);

  // table completely empty
  if (currentTableSize == 0)
  {
    rehash();
    table[a]->contents[table[a]->currentBucketSize++] = key;
    if(nextToSplit == (1ul<<d))
    { 
      ++d;
      nextToSplit = 0;
    };
    return table[a];
  }

  Bucket* currentBucket = table[a];

  // has overflow bucket and the current bucket is full
  while(currentBucket->overflowBucket != nullptr && currentBucket->currentBucketSize == N) 
  {
    currentBucket = currentBucket->overflowBucket;
  }

  // overflow bucket at end is full
  // nowhere left to insert the element, need to rehash the table
  if(currentBucket->currentBucketSize == N && currentBucket->overflowBucket == nullptr) 
  {
    currentBucket->overflowBucket = new Bucket;
    currentBucket = currentBucket->overflowBucket;
    currentBucket->contents[currentBucket->currentBucketSize++] = key;
    if(allocSize >= currentTableSize+1) {
      rehash_noalloc();
    } else
    {
      rehash();
    }

    if(nextToSplit == (1ul<<d))
    { 
      ++d;
      nextToSplit = 0;
    };

    return table[a];
  }

  currentBucket->contents[currentBucket->currentBucketSize++] = key;

  return table[a];
}

// Help function that inserts a key into the table
// never calls rehash
template <typename Key, size_t N>
typename ADS_set<Key,N>::Bucket *ADS_set<Key,N>::insert_noexcept(const key_type &key)
{
  size_type a = h(key, d);
  if (a < nextToSplit) a = h(key, d+1);

  Bucket* currentBucket = table[a];

  // move to first empty bucket or the last bucket
  while(currentBucket->overflowBucket != nullptr && currentBucket->currentBucketSize == N) 
  {
    currentBucket = currentBucket->overflowBucket;
  }

  // bucket is full
  if(currentBucket->currentBucketSize == N && currentBucket->overflowBucket == nullptr) 
  {
    currentBucket->overflowBucket = new Bucket;
    currentBucket = currentBucket->overflowBucket;
    currentBucket->contents[currentBucket->currentBucketSize++] = key;

    return table[a];
  }  
  // Bucket not full
  currentBucket->contents[currentBucket->currentBucketSize++] = key;
  return table[a];
}

// Help function that finds the first Bucket of the row in which the key is saved
template <typename Key, size_t N>
typename ADS_set<Key,N>::Bucket *ADS_set<Key,N>::find_(const key_type &key) const
{
  if(numElements == 0 || currentTableSize == 0) return nullptr;

  size_type a = h(key, d);
  if (a < nextToSplit) a = h(key, d+1);

  for (size_type i {0}; i < table[a]->currentBucketSize; ++i)
  {
    if(key_equal{}(table[a]->contents[i], key)) return table[a];
  }

  Bucket* currentBucket = table[a];

  while (currentBucket->overflowBucket != nullptr)
  {
    currentBucket = currentBucket->overflowBucket;
    for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
    {
      if(key_equal{}(currentBucket->contents[i], key)) return table[a];
    }
  }
  // nothing was found
  return nullptr;
}

// Help function that finds the Bucket of the row in which the key is saved
template <typename Key, size_t N>
typename ADS_set<Key,N>::Bucket *ADS_set<Key,N>::find_ofb(const key_type &key) const
{
  if(numElements == 0) return nullptr;

  size_type a = h(key, d);
  if (a < nextToSplit) a = h(key, d+1);

  for (size_type i {0}; i < table[a]->currentBucketSize; ++i)
  {
    if(key_equal{}(table[a]->contents[i], key)) return table[a];
  }

  Bucket* currentBucket = table[a];

  while (currentBucket->overflowBucket != nullptr)
  {
    currentBucket = currentBucket->overflowBucket;
    for (size_type i {0}; i < currentBucket->currentBucketSize; ++i)
    {
      if(key_equal{}(currentBucket->contents[i], key)) return currentBucket;
    }
  }
  // nothing was found
  return nullptr;
}

// Dump function to print information about the ADS_set to the specified ostream
template <typename Key, size_t N>
void ADS_set<Key,N>::dump(std::ostream &o) const {
  o << "Num Elements: " << numElements << std::endl;
  o << "Table Size: " << currentTableSize << std::endl;
  o << "Alloc Size: " << allocSize << std::endl;
  o << "d: " << d << std::endl;
  o << "nextToSplit is: " << nextToSplit << std::endl;
  for (size_type i{0}; i < currentTableSize; ++i) {
    o << "Bucket " << i << ": ";
    for (size_type j{0}; j < table[i]->currentBucketSize; ++j) {
      o << table[i]->contents[j] << " ";
    }
  
    if(table[i]->overflowBucket != nullptr){
      o << "Overflow Bucket 1: ";
      Bucket* currentBucket = table[i]->overflowBucket;
      for(size_type j{0}; j < currentBucket->currentBucketSize; ++j){
          o << currentBucket->contents[j] << " ";
      }

      int nBucket {2};
      while(currentBucket->overflowBucket != nullptr){
        o << "Overflow Bucket " << nBucket++ << ": ";
        //Bucket* currentBucket = currentBucket->overflowBucket;
        for(size_type j{0}; j < currentBucket->overflowBucket->currentBucketSize; ++j){
          o << currentBucket->overflowBucket->contents[j] << " ";
        }
  
        currentBucket = currentBucket->overflowBucket;
      }
    }
    o << std::endl;
  }
  
  o << "End of Dump." << std::endl;
}

// Iterator class for the ADS_set
template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;
private:
  const key_type* elem;
  Bucket* currentBucket;
  int arrIndex;
  int elemIndex;
  const ADS_set* set;

public:
 // Constructors
  explicit Iterator(const key_type* key, const ADS_set* set, Bucket* cBckt, int arrIdx, int eleIdx) 
  {
    elem = key;
    this->set = set;
    currentBucket = cBckt;
    elemIndex = eleIdx;
    arrIndex = arrIdx;
  }

  explicit Iterator(const ADS_set* set)
  {
    this->set = set;
    this->elem = nullptr;
    this->currentBucket = nullptr;
    this->arrIndex = set->currentTableSize;
    this->elemIndex = -1;
  }

  Iterator()
  {
    this->set = nullptr;
    this->currentBucket = nullptr;
    this->arrIndex = -1;
    this->elemIndex = -1;
  }

  // Invalidates the iterator
  void invalidate()
  {
    this->elem = nullptr;
    this->currentBucket = nullptr;
    this->arrIndex = set->currentTableSize;
    this->elemIndex = -1;
  }

  // access operators
  reference operator*() const { return *elem; }
  pointer operator->() { return elem; } const

  // Functions to advance the iterator
  Iterator &operator++() 
  {
    if(this->set == nullptr) return *this;
    ++elemIndex;
    while(currentBucket->currentBucketSize == static_cast<size_type>(elemIndex)){ //find next bucket with element
        if (currentBucket->overflowBucket == nullptr) //last overflowBucket
        {
          if(static_cast<size_type>(arrIndex+1) == set->currentTableSize)
          {
            // end reached
            invalidate();
            return *this;
          }
          ++arrIndex;
          currentBucket = set->table[arrIndex];
        } else
        {
          currentBucket = currentBucket->overflowBucket;
        }            
        elemIndex = 0;
    }
    
    elem = currentBucket->contents+elemIndex;
    return *this;
  }

  Iterator operator++(int){
    Iterator it(this->elem, this->set, this->currentBucket, this->arrIndex, this->elemIndex);
    operator++();
    return it;
  }

  // (in)equality operators for Iterator
  friend bool operator==(const Iterator &lhs, const Iterator &rhs)
  {
    return (lhs.currentBucket == rhs.currentBucket && lhs.arrIndex == rhs.arrIndex && lhs.elemIndex == rhs.elemIndex && lhs.set == rhs.set);
  }

  friend bool operator!=(const Iterator &lhs, const Iterator &rhs) 
  {
    return !(lhs == rhs);
  }
};

// swaps two ADS_sets
template <typename Key, size_t N> void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) { lhs.swap(rhs); }

#endif // ADS_SET_H