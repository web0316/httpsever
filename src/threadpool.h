#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

template<typename T>
class threadpool {
public:
	threadpool(int thread_num, int request_num);
	bool add_requests(T* request);
	void run();
	~threadpool();

private:
	std::vector<std::thread> threads;
	std::list<T*> work_queue;
	std::mutex queue_mutex;
	std::condition_variable condition;
	int  max_requests;
	bool stop;


};

template<typename T>
void threadpool<T>::run() {
	while (1) {
		T* request;
		{
			std::unique_lock<std::mutex> lock(this->queue_mutex);
			condition.wait(lock, [this] {return this->stop || !(this->work_queue.empty()); });
			if (this->stop&&this->work_queue.empty())
				return;
			request = work_queue.front();
			work_queue.pop_front();
		}
		request->process();
	}
}

template<typename T>
bool threadpool<T>::add_requests(T* request) {
	std::unique_lock<std::mutex> lock(this->queue_mutex); 
	if (work_queue.size() >= max_requests)
		return false;
	work_queue.push_back(request);
	condition.notify_one();
	return true;
}

template<typename T>
inline threadpool<T>::threadpool(int thread_num, int request_num) :max_requests(request_num), stop(false) {
	for (int i = 0; i < thread_num; ++i)
		threads.emplace_back(&threadpool::run, this);

}

template<typename T>
inline threadpool<T>::~threadpool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for (std::thread &worker: threads)
		worker.join();
}

#endif
