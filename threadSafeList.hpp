#pragma once

#include <memory>
#include <mutex>
#include <exception>

template <typename T>
class ThreadSafe_list {
    private:
    struct Node {
        mutable std::mutex mtx;
        std::shared_ptr<T>data;
        std::unique_ptr<Node>next;

        Node() {}
        Node(const T& data) : data(std::make_shared<T>(data)) {}
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
    };

    Node head;
    Node* tail=nullptr;
    mutable std::mutex tail_mtx;
    size_t size;
    mutable std::mutex size_mtx;
    
    public:
    using data_type = T;
    using Self = ThreadSafe_list<T>;

    ThreadSafe_list():size(0) {}
    virtual ~ThreadSafe_list() { size=0; }

    ThreadSafe_list(const ThreadSafe_list&) = delete;
    ThreadSafe_list& operator=(const ThreadSafe_list&) = delete;

    inline size_t length() const;

    void push_front(const T& value);
    void push_back(const T& value);
    void pop_front();
    void pop_front(T& value);
    void pop_back();
    void pop_back(T& value);
    void insert(const size_t& index,const T& value);
    T remove(const size_t& index);
    std::shared_ptr<T> operator[](size_t index);

    template <typename Function>
    void for_each(Function f);

    template <typename Predicate>
    std::shared_ptr<T> find_if(Predicate p);

    template <typename Predicate>
    void remove_if(Predicate p); // 将头节点的下一个节点设置为新的头节点

    // template <typename Compare>
    // void insert_sort(Compare comp);
};

template <typename T>
size_t ThreadSafe_list<T>::length() const {
    std::lock_guard<std::mutex>guard(size_mtx);
    return this->size;
}

template <typename T>
std::shared_ptr<T> ThreadSafe_list<T>::operator[](size_t index) {
    if (index<0 || index>length()-1) {
        throw std::out_of_range("Index out of range");
    }

    if (index==length()-1) {
        std::lock_guard<std::mutex>guard(tail_mtx);
        return tail->data;
    }

    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    for (int i=0;i<=index;++i) {
        Node* const next=curr->next.get();
        std::unique_lock<std::mutex>next_lk(next->mtx);
        lk.unlock();

        curr=next;
        lk=std::move(next_lk);
    }

    return curr->data;
}

template <typename T>
void ThreadSafe_list<T>::push_front(const T& value) {
    auto new_node=std::make_unique<Node>(value);
    std::lock_guard<std::mutex>guard(head.mtx);
    new_node->next=std::move(head.next);
    head.next=std::move(new_node);

    if (tail==nullptr) {
        std::lock_guard<std::mutex>guard(this->tail_mtx);
        tail=this->head.next.get();
    }

    {
        std::lock_guard<std::mutex>guard(this->size_mtx);
        this->size++;
    }
}

template <typename T>
void ThreadSafe_list<T>::pop_back(T& value) {
    size_t target_size;
    {
        std::lock_guard<std::mutex>guard(size_mtx);
        target_size=this->size;
    }

    value=remove(target_size-1);
}

template <typename T>
void ThreadSafe_list<T>::pop_back() {
    data_type temp;
    pop_back(temp);
}

template <typename T>
template <typename Function>
void ThreadSafe_list<T>::for_each(Function f) {
    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    while (Node* const next=curr->next.get()) {
        std::unique_lock<std::mutex>next_lk(next->mtx);
        lk.unlock();

        f(*next->data);

        curr=next;
        lk=std::move(next_lk);
    }
}

template <typename T>
void ThreadSafe_list<T>::push_back(const T& value) {
    auto new_node=std::make_unique<Node>(value);
    std::lock_guard<std::mutex>guard(this->tail_mtx);

    if (tail==nullptr) {
        std::lock_guard<std::mutex>head_guard(head.mtx);
        head.next=std::move(new_node);
        tail=head.next.get();
    } else {
        tail->next=std::move(new_node);
        tail=tail->next.get();
    }

    {
        std::lock_guard<std::mutex>guard(this->size_mtx);
        this->size++;
    }
}

template <typename T>
void ThreadSafe_list<T>::pop_front() {
    data_type temp;
    pop_front(temp);
}

template <typename T>
void ThreadSafe_list<T>::pop_front(T& value) {
    std::lock(head.mtx, size_mtx);
    std::lock_guard<std::mutex> lock_head(head.mtx, std::adopt_lock);
    std::lock_guard<std::mutex> lock_size(size_mtx, std::adopt_lock);

    if (head.next) {
        auto old_head = std::move(head.next);
        value=*old_head->data;
        head.next = std::move(old_head->next);

        if (!head.next) { // 如果链表为空，更新 tail 指针
            std::lock_guard<std::mutex> lock_tail(tail_mtx);
            tail = nullptr;
        }

        --size;
        old_head.reset();
    }
}

template <typename T>
template <typename Predicate>
std::shared_ptr<T> ThreadSafe_list<T>::find_if(Predicate p) {
    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    while (Node* const next=curr->next.get()) {
        std::unique_lock<std::mutex>next_lk(next->mtx);
        lk.unlock();

        if (p(*next->data)) {
            return next->data;
        }

        curr=next;
        lk=std::move(next_lk);
    }
}

template <typename T>
template <typename Predicate>
void ThreadSafe_list<T>::remove_if(Predicate p) {
    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    while (Node* const next=curr->next.get()) {
        std::unique_lock<std::mutex>next_lk(next->mtx);

        if (p(*next->data)) {
            auto old_next=std::move(curr->next);
            curr->next=std::move(next->next);
            next_lk.unlock();
            old_next.reset();
        } else {
            lk.unlock();
            curr=next;
            lk=std::move(next_lk);
        }
    }
}

template <typename T>
void ThreadSafe_list<T>::insert(const size_t& index,const T& value) {
    if (index == 0) {
        push_front(value);
        return;
    }

    {
        std::lock_guard<std::mutex> size_guard(size_mtx);
        if (index>size-1) throw std::out_of_range("Index out of range");
    }


    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    for (int i=0;i<index;++i) {
        Node* const next=curr->next.get();
        std::unique_lock<std::mutex>next_lk(next->mtx);

        lk.unlock();
        curr=next;
        lk=std::move(next_lk);
    }

    auto new_node=std::make_unique<Node>(value);
    auto old_next=std::move(curr->next);
    new_node->next=std::move(old_next);
    curr->next=std::move(new_node);

    {
        std::lock_guard<std::mutex>guard(size_mtx);
        this->size++;
    }
}

template <typename T>
T ThreadSafe_list<T>::remove(const size_t& index) {
    if (index>length()-1) {
        throw std::out_of_range("Index out of range");
    }

    Node* curr=&head;
    std::unique_lock<std::mutex>lk(head.mtx);

    for (int i=0;i<index;++i) {
        Node* const next=curr->next.get();
        std::unique_lock<std::mutex>next_lk(next->mtx);
        lk.unlock();

        curr=next;
        lk=std::move(next_lk);
    }

    Node* const next=curr->next.get();
    std::unique_lock<std::mutex>next_lk(next->mtx);
    auto removed_node=std::move(curr->next);
    curr->next=std::move(next->next);
    next_lk.unlock();

    if (!curr->next) {
        std::lock_guard<std::mutex> tail_lock(tail_mtx);
        tail = curr;
    }

    {
        std::lock_guard<std::mutex> size_lock_guard(size_mtx);
        --size;
    }
    data_type value=*(removed_node->data);
    removed_node.reset();
    return value;
}