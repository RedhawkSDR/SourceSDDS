/*
 * SmartPacketBuffer.h
 *
 *  Created on: Mar 29, 2016
 *      Author: ylbagou
 */

#ifndef SMARTPACKETBUFFER_H_
#define SMARTPACKETBUFFER_H_

#include <boost/circular_buffer.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>
#include <boost/call_traits.hpp>
#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <deque>


/**
 * Two deques of boost smart pointers
 * One deque full of empty buffers to be used
 * One deque where filled buffers are placed.
 * You MUST follow this cycle: pop_empty_buffer -> push_full_buffer -> pop_full_buffer -> recycle_buffer
 * Memory is only allocated at construction so if you do not follow the above cycle and a smart pointer goes
 * out of scope it will free itself.
 *
 * The locking here is done with conditional variables so that it should be quick however if this ends up being
 * a point of thread contention it could be reimplemented using a no-wait no-locking queue. I've tested it at
 * 3Gbps so I do not think that will become an issue but see this post if someone in the future wants to go
 * down that path: http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448?pgno=2
 * There are better options than drdobbs example however most require C++11 and his example only requires c++0x
 * There is no license or copyright shown however after emailing him regarding use he responded with
 * "Hi Youssef, it’s “as is” for you to use. Thanks,"
 *
 */
template <class T >
class SmartPacketBuffer {
public:

	typedef boost::shared_ptr<T> TypePtr;
    typedef std::deque<TypePtr> container_type;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::value_type value_type;

    explicit SmartPacketBuffer():m_shuttingDown(false) {}

    void initialize(size_type capacity) {
    	boost::unique_lock<boost::mutex> lock(m_empty_buffer_mutex);
    	size_t i;

    	// Allocate the memory and fill the empty buffers
    	for (i = 0; i < capacity; ++i) {
    		m_empty_buffers.push_back(TypePtr(new T()));
    	}
    	lock.unlock();
    }

    void destroyBuffers() {
    	m_shuttingDown = true;
    	m_no_empty_buffers.notify_all();
    	m_no_full_buffers.notify_all();

		boost::unique_lock<boost::mutex> lock1(m_full_buffer_mutex);
    	m_full_buffers.clear();
    	lock1.unlock();

    	boost::unique_lock<boost::mutex> lock2(m_empty_buffer_mutex);
    	m_empty_buffers.clear();
		lock2.unlock();

		m_shuttingDown = false;
    }


    TypePtr pop_empty_buffer() {
    	boost::unique_lock<boost::mutex> lock(m_empty_buffer_mutex);
    	m_no_empty_buffers.wait(lock, boost::bind(&SmartPacketBuffer<T>::empties_available, this));
    	if (m_shuttingDown) {return NULL;}
    	TypePtr retVal = *m_empty_buffers.begin();
    	m_empty_buffers.pop_front();
    	lock.unlock();
    	return retVal;
    }

    /**
     * Fill the provided vector until it is len in size of empty buffers.
     */
    void pop_empty_buffers(std::vector<TypePtr> &vec, size_t len) {
    		// Maybe they have what they want already
        	if (vec.size() >= len)
        		return;

        	size_t request = len - vec.size();

        	boost::unique_lock<boost::mutex> lock(m_empty_buffer_mutex);
        	m_no_empty_buffers.wait(lock, boost::bind(&SmartPacketBuffer<T>::empties_available, this, request));
        	if (m_shuttingDown) {return;}

        	while (vec.size() != len) {
        		vec.push_back(*m_empty_buffers.begin());
				m_empty_buffers.pop_front();
        	}

        	lock.unlock();
        }

    void push_full_buffer(TypePtr b) {
    	boost::unique_lock<boost::mutex> lock(m_full_buffer_mutex);
    	m_full_buffers.push_back(b);
    	lock.unlock();
    	m_no_full_buffers.notify_one();
    }

    /**
     * Pushes all the buffers contained in vec onto the full buffer deque
     * and clears the given vector.
     */
    void push_full_buffers(std::vector<TypePtr> &vec) {
    	boost::unique_lock<boost::mutex> lock(m_full_buffer_mutex);
    	m_full_buffers.insert(m_full_buffers.end(), vec.begin(), vec.end());
    	vec.clear();
    	lock.unlock();
		m_no_full_buffers.notify_one();
    }

    TypePtr pop_full_buffer() {
    	boost::unique_lock<boost::mutex> lock(m_full_buffer_mutex);
		m_no_full_buffers.wait(lock, boost::bind(&SmartPacketBuffer<T>::full_available, this));
		if (m_shuttingDown) {return NULL;}
		TypePtr retVal = *m_full_buffers.begin();
		m_full_buffers.pop_front();
		lock.unlock();
		return retVal;
	}


    /**
     * Fill the provided vector until it is len in size of full buffers.
     */
    void pop_full_buffers(std::vector<TypePtr> &vec, size_t len) {
		// Maybe they have what they want already
    	if (vec.size() >= len)
    		return;

    	size_t request = len - vec.size();

    	boost::unique_lock<boost::mutex> lock(m_full_buffer_mutex);
		m_no_full_buffers.wait(lock, boost::bind(&SmartPacketBuffer<T>::full_available, this, request));
		if (m_shuttingDown) {return;}

    	while (vec.size() != len) {
    		vec.push_back(*m_full_buffers.begin());
    		m_full_buffers.pop_front();
    	}

		lock.unlock();
	}

    void recycle_buffer(TypePtr b) {
    	boost::unique_lock<boost::mutex> lock(m_empty_buffer_mutex);
    	m_empty_buffers.push_back(b);
    	lock.unlock();
    	m_no_empty_buffers.notify_one();
    }

    void recycle_buffers(std::vector<TypePtr> &vec) {
    	boost::unique_lock<boost::mutex> lock(m_empty_buffer_mutex);
    	m_empty_buffers.insert(m_empty_buffers.end(), vec.begin(), vec.end());
    	vec.clear();
    	lock.unlock();
    	m_no_empty_buffers.notify_one();
    }

private:
    SmartPacketBuffer(const SmartPacketBuffer&);              // Disabled copy constructor
    SmartPacketBuffer& operator = (const SmartPacketBuffer&); // Disabled assign operator
    bool m_shuttingDown;

    /**
     * If we are shutting down we need to just open the gates up and let the threads run.
     * If not we'll have folks blocking on us
     */
    bool empties_available() const { return m_empty_buffers.size() > 0 					|| m_shuttingDown; }
    bool empties_available(size_t num) const { return m_empty_buffers.size() >= num 	|| m_shuttingDown; }
    bool full_available() const { return m_full_buffers.size() > 0 						|| m_shuttingDown; }
    bool full_available(size_t num) const { return m_full_buffers.size() >= num 		|| m_shuttingDown; }


    container_type m_empty_buffers;
    container_type m_full_buffers;
    boost::mutex m_empty_buffer_mutex;
    boost::mutex m_full_buffer_mutex;
    boost::condition_variable m_no_empty_buffers;
    boost::condition_variable m_no_full_buffers;
};

#endif /* PACKETBUFFER_H_ */
