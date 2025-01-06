#pragma once

#include <memory>
#include <mutex>

template <typename T>
class ThreadSafe_list {
    private:
    struct Node {
        std::mutex mtx;
        std::shared_ptr<T>data;
        std::unique_ptr<Node>next;

        Node() {}
        Node(const T& data) : data(std::make_shared<T>(data)) {}
    };

    Node head;
    Node* tail=nullptr;
    std::mutex tail_mtx;
    size_t size=0;
    std::mutex size_mtx;
    
    public:
    using data_type = T;

    ThreadSafe_list() {}
    virtual ~ThreadSafe_list() { size=0; }

    ThreadSafe_list(const ThreadSafe_list& other) = delete;
    ThreadSafe_list& operator=(const ThreadSafe_list& other) = delete;

    void push_front(const T& value);
    void push_back(const T& value);

    template <typename Function>
    void for_each(Function f);

    template <typename Predicate>
    std::shared_ptr<T> find_if(Predicate p);

    template <typename Predicate>
    void remove_if(Predicate p);
};

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

            {
                std::lock_guard<std::mutex>guard(this->size_mtx);
                this->size--;
            }
        } else {
            lk.unlock();
            curr=next;
            lk=std::move(next_lk);
        }
    }
}
