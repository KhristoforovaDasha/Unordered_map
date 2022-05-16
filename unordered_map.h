#pragma once
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

template<size_t N>
class alignas(::max_align_t) StackStorage {
 public:
  StackStorage() = default;

  StackStorage(const StackStorage&) = delete;

  StackStorage& operator=(const StackStorage&) = delete;

  uint8_t* allocate(size_t count, size_t alignment) {
    position_ += (alignment - position_ % alignment) % alignment;
    uint8_t* result = buffer_ + position_;
    position_ += count;
    return result;
  }

 private:
  uint8_t buffer_[N];
  size_t position_ = 0;
};

template<typename Type, size_t N>
class StackAllocator {
 public:
  using value_type = Type;
  template<typename Node>
  struct rebind { using other = StackAllocator<Node, N>; };

  StackAllocator() = delete;

  StackAllocator(StackStorage<N>& other) : store_(&other) {}

  template<typename OtherType>
  StackAllocator(const StackAllocator<OtherType, N>& other)
      :store_(other.store_) {}

  StackAllocator& operator=(const StackAllocator& other) = default;

  Type* allocate(size_t count) {
    return reinterpret_cast<Type*>(store_->allocate(count * sizeof(Type),
                                                    alignof(Type)));
  }

  void deallocate(Type* pointer, size_t count) {
    if (pointer || count) {

    }
  }

 private:
  template<typename AllType, size_t AllN>
  friend
  class StackAllocator;
  StackStorage<N>* store_;
};

template<typename Type, typename Allocator = std::allocator<Type> >
class List {
 private:
  template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
  friend
  class UnorderedMap;
  struct alignas(::max_align_t) BaseNode {
    BaseNode() = default;

    BaseNode(BaseNode* next, BaseNode* prev) : next_(next), prev_(prev) {}

    BaseNode* next_ = nullptr;
    BaseNode* prev_ = nullptr;
  };
  struct Node : BaseNode {
    Node() = default;

    template<typename ...Args>
    Node(Args... args) : value_(std::forward<Args>(args)...) {}

    Node(const Type& value) : value_(value) {}

    Node(Type&& value) : value_(std::move(value)) {}

    Node(const Node& node) : value_(node.value_) {}

    Node(Node&& node) : value_(std::move(node.value_)) {}

    Type value_;
  };

 public:
  template<bool isConst>
  class common_iterator {
   public:
    template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
    friend
    class UnorderedMap;
    friend class List;
    using difference_type = std::ptrdiff_t;;
    using iterator_category = std::bidirectional_iterator_tag;
    using pointer = std::conditional_t<isConst, const Type*, Type*>;
    using reference = std::conditional_t<isConst, const Type&, Type&>;
    using value_type = std::conditional_t<isConst, const Type, Type>;

    common_iterator() = default;

    explicit common_iterator(const BaseNode* it) : it_(const_cast<BaseNode*>(it)) {}

    common_iterator(const common_iterator& copy) : it_(copy.it_) {}

    common_iterator& operator++() {
      it_ = it_->next_;
      return *this;
    }

    common_iterator operator++(int) {
      common_iterator copy = *this;
      ++(*this);
      return copy;
    }

    common_iterator& operator--() {
      it_ = it_->prev_;
      return *this;
    }

    common_iterator operator--(int) {
      common_iterator copy = *this;
      --(*this);
      return copy;
    }

    bool operator==(const common_iterator<isConst>& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const common_iterator<isConst>& other) const {
      return it_ != other.it_;
    }

    reference operator*() const {
      return static_cast<std::conditional_t<isConst,
                                            const Node*,
                                            Node*> >(it_)->value_;
    }

    pointer operator->() const {
      return &static_cast<std::conditional_t<isConst,
                                             const Node*,
                                             Node*> >(it_)->value_;
    }

    operator common_iterator<true>() {
      return common_iterator<true>(it_);
    }

   protected:
    BaseNode* it_;
  };
  using iterator = common_iterator<false>;
  using const_iterator = common_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() {
    return iterator(fake_node_.next_);
  };

  const_iterator begin() const {
    return iterator(fake_node_.next_);
  };

  const_iterator cbegin() const {
    return const_iterator(fake_node_.next_);
  };

  iterator end() {
    return iterator(&fake_node_);
  };

  const_iterator end() const {
    return const_iterator(&fake_node_);
  };

  const_iterator cend() const {
    return const_iterator(&fake_node_);
  };

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  };

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  };

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  };

  reverse_iterator rend() {
    return reverse_iterator(begin());
  };

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  };

  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  };

  using NodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<
      Node>;
  using NodeAllocTraits = std::allocator_traits<NodeAlloc>;

  List(const Allocator& alloc = Allocator()) : alloc_(alloc) {}

  List(size_t count,
       const Type& value,
       const Allocator& alloc = Allocator())
      : alloc_(alloc) {
    try {
      while (count-- != 0) {
        construct_node(end(), value);
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List(size_t count, const Allocator& alloc = Allocator()) : alloc_(alloc) {
    try {
      while (count-- != 0) {
        construct_node(end());
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List(const List& other)
      : alloc_(std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc_)) {
    try {
      for (auto it = other.begin(); it != other.end(); ++it) {
        construct_node(end(), *it);
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List(List&& other)
      : size_(other.size_),
        fake_node_(other.fake_node_.next_, other.fake_node_.prev_), alloc_(std::move(other.alloc_)) {
    other.fake_node_.next_ = other.fake_node_.prev_ = &other.fake_node_;
    fake_node_.next_->prev_ = fake_node_.prev_->next_ = &fake_node_;
    other.size_ = 0;
  }

  List& operator=(List&& other) {
    List copy(std::move(other));
    copy.alloc_ =
        (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ? other.alloc_ : alloc_);

    swap(std::move(copy));
    return *this;
  }

  List& operator=(const List& other) {
    List copy(std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value ? other.alloc_ : alloc_);
    for (auto it = other.begin(); it != other.end(); ++it) {
      copy.push_back(*it);
    }
    swap(copy);
    return *this;
  }

  Allocator get_allocator() { return alloc_; }

  size_t size() const { return size_; }

  void push_back(const Type& value) {
    construct_node(end(), value);
  }

  void push_back(Type&& value) {
    construct_node(end(), std::move(value));
  }

  void push_front(const Type& value) {
    construct_node(begin(), value);
  }

  void push_front(Type&& value) {
    construct_node(begin(), std::move(value));
  }

  void pop_back() {
    erase(--end());
  }

  void pop_front() {
    erase(begin());
  }

  void insert(const_iterator it, const Type& elem) {
    construct_node(it, elem);
  }

  void insert(const_iterator it, Type&& elem) {
    construct_node(it, std::move(elem));
  }

  void erase(const const_iterator& it) {
    BaseNode* node = it.it_;
    node->prev_->next_ = node->next_;
    node->next_->prev_ = node->prev_;
    delete_node(reinterpret_cast<Node*>(node));
    --size_;
  }

  template<typename ...Args>
  void emplace(const const_iterator& it, Args&& ... args) {
    construct_node(it, std::forward<Args>(args)...);
  }

  ~List() {
    delete_list();
  }

 private:
  void swap(List& other) {
    if (this != &other) {
      auto next = fake_node_.next_, prev = fake_node_.prev_;
      auto other_next = other.fake_node_.next_, other_prev = other.fake_node_.prev_;
      std::swap(size_, other.size_);
      std::swap(next->prev_, other_next->prev_);
      std::swap(prev->next_, other_prev->next_);
      std::swap(fake_node_, other.fake_node_);
      std::swap(alloc_, other.alloc_);
    }
  }

  void swap(List&& other) {
    if (this != &other) {
      size_ = other.size_;
      other.size_ = 0;
      std::swap(fake_node_, other.fake_node_);
      other.fake_node_.next_ = other.fake_node_.prev_ = &other.fake_node_;
      fake_node_.next_->prev_ = fake_node_.prev_->next_ = &fake_node_;
    }
  }

  void delete_node(Node* node) {
    NodeAllocTraits::destroy(alloc_, node);
    NodeAllocTraits::deallocate(alloc_, node, 1);
  }

  void delete_list() {
    iterator it = begin();
    while (size_-- != 0) {
      iterator current = it++;
      delete_node(reinterpret_cast<Node*>(current.it_));
    }
  }

  template<typename ...Args>
  void construct_node(const const_iterator& iter, Args&& ... args) {
    Node* node = NodeAllocTraits::allocate(alloc_, 1);
    try {
      NodeAllocTraits::construct(alloc_, node, std::forward<Args>(args)...);
    } catch (...) {
      NodeAllocTraits::deallocate(alloc_, node, 1);
      throw;
    }
    construct_node_pointers(iter, node);
  }

  void construct_node(const const_iterator& iter) {
    Node* node = NodeAllocTraits::allocate(alloc_, 1);
    try {
      NodeAllocTraits::construct(alloc_, node);
    } catch (...) {
      NodeAllocTraits::deallocate(alloc_, node, 1);
      throw;
    }
    construct_node_pointers(iter, node);
  }

  void construct_node_pointers(const const_iterator& iter, Node* node) {
    BaseNode* pointer = iter.it_;
    BaseNode* pointer_prev = iter.it_->prev_;
    pointer->prev_ = node;
    pointer_prev->next_ = node;
    node->prev_ = pointer_prev;
    node->next_ = pointer;
    ++size_;
  }

  size_t size_ = 0;
  BaseNode fake_node_ = BaseNode(&fake_node_, &fake_node_);
  NodeAlloc alloc_;
};

template<typename Key, typename Value, typename Hash = std::hash<Key>,
    typename Equal = std::equal_to<Key>, typename Alloc = std::allocator<std::pair<const Key, Value> > >
class UnorderedMap {
 private:
  using NodeType = std::pair<const Key, Value>;
  struct ListNode {
    NodeType node_;
    size_t hash_ = 0;

    template<typename ...Args>
    ListNode(Args&& ... args, size_t hash = 0) : node_(std::forward<Args>(args)...), hash_(hash) {}

    ListNode(ListNode&& other) : node_(std::move(other.node_)), hash_(other.hash_) {}

    ListNode(const ListNode& other) : node_(other.node_), hash_(other.hash_) {}

    ListNode(const NodeType& node, size_t hash = 0) : node_(node), hash_(hash) {}

    ListNode(NodeType&& node, size_t hash = 0) : node_(std::move(node)), hash_(hash) {}

    ListNode(const Key& key, const Value& value, size_t hash = 0) : node_(std::make_pair(key, value)), hash_(hash) {}

    ListNode(Key&& key, Value&& value, size_t hash = 0) : node_(std::make_pair(std::move(key), std::move(value))),
                                                          hash_(hash) {}

    ListNode(const Key& key, Value&& value, size_t hash = 0) : node_(std::make_pair(key, std::move(value))),
                                                               hash_(hash) {}
  };
  using NodeAllocType = typename std::allocator_traits<Alloc>::template rebind_alloc<ListNode>;
  using NodeAllocTraitsValue = std::allocator_traits<NodeAllocType>;
  template<bool isConst>
  friend
  class List<ListNode, NodeAllocType>::common_iterator;
  using ListConstIterator = typename List<ListNode, NodeAllocType>::const_iterator;
  using ListIterator = typename List<ListNode, NodeAllocType>::iterator;
  using Node = typename List<ListNode, NodeAllocType>::Node;
 public:
  template<bool isConst>
  class common_iterator {
   public:
    friend class UnorderedMap;
    using Iterator = std::conditional_t<isConst, ListConstIterator, ListIterator>;
    using difference_type = std::ptrdiff_t;;
    using iterator_category = std::forward_iterator_tag;
    using pointer = std::conditional_t<isConst, const NodeType*, NodeType*>;
    using reference = std::conditional_t<isConst, const NodeType&, NodeType&>;
    using value_type = std::conditional_t<isConst, const NodeType, NodeType>;

    common_iterator(const Iterator& it) : it_(it) {}

    common_iterator(const common_iterator& copy) : it_(copy.it_) {}

    common_iterator& operator++() {
      ++it_;
      return *this;
    }

    common_iterator operator++(int) {
      common_iterator copy = *this;
      ++(*this);
      return copy;
    }

    bool operator==(const common_iterator<isConst>& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const common_iterator<isConst>& other) const {
      return it_ != other.it_;
    }

    reference operator*() const {
      return it_->node_;
    }

    pointer operator->() const {
      return &(it_->node_);
    }

    operator common_iterator<true>() {
      return common_iterator<true>(it_);
    }

   private:
    Iterator it_;
  };
  using iterator = common_iterator<false>;
  using const_iterator = common_iterator<true>;

  UnorderedMap() {
    value_.assign(vec_size, nullptr);
  }

  UnorderedMap(const UnorderedMap& other) :
      allocator_(NodeAllocTraits::select_on_container_copy_construction(other.allocator_)),
      alloc_key_value_(std::allocator_traits<Alloc>::select_on_container_copy_construction(other.alloc_key_value_)) {
    value_.resize(other.value_.size(), nullptr);
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(*it);
    }
  }

  UnorderedMap(UnorderedMap&& other) : value_(std::move(other.value_)),
                                       key_(std::move(other.key_)), allocator_(std::move(other.allocator_)),
                                       alloc_key_value_(std::move(other.alloc_key_value_)) {}

  UnorderedMap& operator=(UnorderedMap&& other) {
    UnorderedMap copy(std::move(other));
    swap(copy);
    if (NodeAllocTraits::propagate_on_container_move_assignment::value) {
      allocator_ = std::move(copy.allocator_);
    }
    if (std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value) {
      alloc_key_value_ = std::move(copy.alloc_key_value_);
    }
    return *this;
  }

  UnorderedMap& operator=(const UnorderedMap& other) {
    UnorderedMap copy(other);
    swap(copy);
    if (NodeAllocTraits::propagate_on_container_copy_assignment::value) {
      allocator_ = other.allocator_;
    }
    if (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value) {
      alloc_key_value_ = other.alloc_key_value_;
    }
    return *this;
  }

  void swap(UnorderedMap& other) {
    std::swap(value_, other.value_);
    std::swap(max_factor, other.max_factor);
    key_.swap(other.key_);
  }

  ~UnorderedMap() {}

  size_t size() const noexcept { return key_.size(); }

  Value& at(const Key& key) {
    size_t hash = hash_(key);
    size_t hash_mod = hash % value_.size();
    if (value_[hash_mod] == nullptr) {
      throw std::out_of_range("out of range");
    }
    auto iter = ListIterator(value_[hash_mod]);
    while (iter != key_.end() && iter->hash_ == hash) {
      if (equal_(iter->node_.first, key)) {
        return iter->node_.second;
      }
      ++iter;
    }
    return iter->node_.second;
  }

  const Value& at(const Key& key) const {
    size_t hash = hash_(key);
    size_t hash_mod = hash % value_.size();
    if (value_[hash_mod] == nullptr) throw std::out_of_range("out of range");
    auto iter = ListIterator(value_[hash_mod]);
    while (iter != key_.end() && iter->hash_ == hash) {
      if (equal_(iter->node_.first, key)) {
        return iter->node_.second;
      }
      ++iter;
    }
    return iter->node_.second;
  }

  Value& operator[](const Key& key) {
    try {
      return at(key);
    } catch (...) {
      Value default_value = Value();
      auto iter = emplace(key, std::move(default_value));
      return iter.first->second;
    }
  }

  std::pair<iterator, bool> insert(const NodeType& node) {
      return emplace(node.first, node.second);
  }

  std::pair<iterator, bool> insert(NodeType&& node) {
    if (find(node.first) == end()) {
      return emplace(std::move(const_cast<Key&>(node.first)), std::move(node.second));
    } else {
      return std::make_pair(end(), false);
    }
  }

  template<typename InputIterator>
  void insert(InputIterator iter_begin, InputIterator iter_end) {
    for (auto iter = iter_begin; iter != iter_end; ++iter) {
      insert(*iter);
    }
  }

  template<typename InputIterator>
  void erase(InputIterator iter_begin, InputIterator iter_end) {
    for (auto iter = iter_begin; iter != iter_end;) {
      auto copy_iter = iter;
      ++iter;
      erase(copy_iter->first);
    }
  }

  template<typename InputIterator>
  void erase(InputIterator iter) {
    erase(iter->first);
  }

  void erase(const Key& key) {
    size_t hash = hash_(key) % value_.size();
    auto iter = find(key);
    if (iter != end()) {
      Node* value_pointer;
      auto copy_iter = iter;
      if ((++copy_iter) != end() && (copy_iter.it_)->hash_ == hash)
        value_pointer = reinterpret_cast<Node*>(copy_iter.it_.it_);
      else value_pointer = nullptr;
      key_.erase(iter.it_);
      value_[hash] = value_pointer;
    }
  }

  size_t max_size() const noexcept {
    return (1 << 31);
  }

  double load_factor() const noexcept {
    return size() / value_.size();
  }

  float max_load_factor() const noexcept {
    return max_factor;
  }

  void max_load_factor(float factor) {
    max_factor = factor;
  }

  void rehash() {
    if (load_factor() >= max_factor) {
      reserve(2 * size() / max_factor);
    }
  }

  void reserve(size_t count) {
    value_.assign(count, nullptr);
    reconstruct();
  }

  ListIterator find_place(size_t hash) {
    if (value_[hash] == nullptr) {
      return key_.end();
    }
    return ListIterator(value_[hash]);
  }

  void reconstruct() {
    List<ListNode, NodeAllocType> new_list;
    key_.swap(new_list);
    for (auto iter = new_list.begin(); iter != new_list.end();) {
      ListIterator copy_iter = iter;
      ++iter;
      Node* list_node = reinterpret_cast<Node*>(copy_iter.it_);
      size_t hash = copy_iter->hash_ % value_.size();
      ListIterator place = find_place(hash);
      erase_node(list_node);
      --new_list.size_;
      emplace_elem(place, list_node);
      if (value_[hash] == nullptr) {
        value_[hash] = list_node;
      }
    }
  }

  void erase_node(Node*& list_node) {
    list_node->prev_->next_ = list_node->next_;
    list_node->next_->prev_ = list_node->prev_;
    list_node->prev_ = nullptr;
    list_node->next_ = nullptr;
  }

  void emplace_elem(ListIterator iter, Node*& list_node) {
    iter.it_->next_->prev_ = list_node;
    list_node->next_ = iter.it_->next_;
    iter.it_->next_ = list_node;
    list_node->prev_ = iter.it_;
    ++key_.size_;
  }

  template<typename ...Args>
  std::pair<iterator, bool> emplace(Args&& ... args) {
    Node* list_node = NodeAllocTraits::allocate(allocator_, 1);
    try {
      std::allocator_traits<Alloc>::construct(alloc_key_value_, &list_node->value_.node_, std::forward<Args>(args)...);
    } catch(...){
      std::allocator_traits<Alloc>::deallocate(alloc_key_value_, &list_node->value_.node_, 1);
    }
    size_t hash = hash_(list_node->value_.node_.first);
    list_node->value_.hash_ = hash;
    hash %= value_.size();
    ListIterator iter = find_place(hash);
    emplace_elem(iter, list_node);
    if (value_[hash] == nullptr) {
      value_[hash] = list_node;
    }
    rehash();
    return std::make_pair(iterator(ListIterator(list_node)), true);
  }

  iterator begin() {
    return iterator(key_.begin());
  }

  const_iterator begin() const {
    return const_iterator(key_.begin());
  }

  const_iterator cbegin() const {
    return const_iterator(key_.begin());
  }

  iterator end() {
    return iterator(key_.end());
  }

  const_iterator end() const {
    return const_iterator(key_.end());
  }

  const_iterator cend() const {
    return const_iterator(key_.end());
  }

  iterator find(const Key& key) {
    size_t hash = hash_(key);
    size_t hash_mod = hash_(key) % value_.size();
    if (value_[hash_mod] == nullptr) {
      return iterator(key_.end());
    }
    auto iter = ListIterator(value_[hash_mod]);
    ListIterator end = key_.end();
    while (iter != end && iter->hash_ == hash) {
      if (equal_(iter->node_.first, key)) {
        return iterator(iter);
      }
      ++iter;
    }
    return iterator(end);
  }

  const_iterator find(const Key& key) const {
    size_t hash = hash_(key);
    size_t hash_mod = hash_(key) % value_.size();
    if (value_[hash_mod] == nullptr) {
      return const_iterator(key_.end());
    }
    auto iter = ListIterator(value_[hash_mod]);
    ListIterator end = key_.end();
    while (iter != end && iter->hash_ == hash) {
      if (equal_(iter->node_.first, key)) {
        return const_iterator(iter);
      }
      ++iter;
    }
    return const_iterator(end);
  }

 private:
  size_t vec_size = 8;
  float max_factor = 1.0;
  using NodeAllocVector = typename std::allocator_traits<Alloc>::template rebind_alloc<Node*>;
  using NodeAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
  using NodeAllocTraits = std::allocator_traits<NodeAlloc>;
  std::vector<Node*, NodeAllocVector> value_;
  List<ListNode, NodeAllocType> key_;
  Hash hash_;
  Equal equal_;
  NodeAlloc allocator_;
  Alloc alloc_key_value_;
};