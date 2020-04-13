#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace tcp {
	template <typename T>
	class SafeQueue;
}

template <typename T>
class tcp::SafeQueue
{
public:
	unsigned int count()
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		unsigned int c = queue_.size();
		mlock.unlock();
		return c;
	}

	T pop()
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		{
			cond_.wait(mlock);
		}
		auto item = queue_.front();
		queue_.pop();
		return item;
	}

	void pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		{
			cond_.wait(mlock);
		}
		item = queue_.front();
		queue_.pop();
	}

	bool try_pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);

		if (queue_.size() == 0) {
			mlock.unlock();
			return false;
		}

		item = queue_.front();
		queue_.pop();

		mlock.unlock();
		return true;
	}



	void push(const T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		queue_.push(item);
		mlock.unlock();
		cond_.notify_one();
	}

	void push(T&& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		queue_.push(std::move(item));
		mlock.unlock();
		cond_.notify_one();
	}

private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

#endif // SAFEQUEUE_H
