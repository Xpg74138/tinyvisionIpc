#pragma once

#include <queue>
#include <mutex>

template<typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue()
	{

	}

    ~ThreadSafeQueue()
	{
		std::lock_guard<std::mutex> lck(mut);
        while (!queue.empty()) {
			// T * t = queue.front();
			queue.pop();
		}
	}

    int enqueue(T * t) {
		std::lock_guard<std::mutex> lck(mut);

		queue.push(t);

		return 0;
	}

    int dequeue(T * * t) {
		std::lock_guard<std::mutex> lck(mut);

        if (!queue.empty()) {
			*t = queue.front();
			queue.pop();
			return 0;
		}

		return -1;
	}

    int size() {
		std::lock_guard<std::mutex> lck(mut);
		return queue.size();
	}
	
	size_t getDataSize() {
        std::lock_guard<std::mutex> lck(mut);
        return dataSize;
    }

    void setDataSize(size_t size) {
        std::lock_guard<std::mutex> lck(mut);
        dataSize = size;
    }

    void incDataSize(size_t size) {
        std::lock_guard<std::mutex> lck(mut);
        dataSize += size;
    }

    void subDataSize(size_t size) {
        std::lock_guard<std::mutex> lck(mut);
        dataSize -= size;
    }

private:
	std::queue<T *> queue;
	std::mutex mut;
	size_t dataSize = 0;
};
