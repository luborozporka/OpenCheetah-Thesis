/* ENERGY CONSUMPTION.hpp
 *   by Lut99
 *
 * Created:
 *   09 Oct 2024, 17:17:49
 * Last edited:
 *   17 Apr 2025, 10:58:23
 * Auto updated?
 *   Yes
 *
 * Description:
 *   A small library that can be used to measure energy consumption in the
 *   background of other C++ code running.
**/

#ifndef _ENERGY_CONSUMPTION_HPP
#define _ENERGY_CONSUMPTION_HPP

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


/* Our own wrapper around a conditional variable to create a semaphore-like interface.
 * 
 * Semaphores are used by one thread (main in our case) to notify some other thread (the measurement
 * one) that something has happened. In our case, we use this to "interrupt" its waiting, wake up,
 * and notice it should stop measuring.
 * 
 * # How it works
 * This semaphore is based on _conditional variables_. These are essentially a notification
 * mechanism provided by the OS to allow a thread to sleep (i.e., be non-active) until another
 * thread sends it a notification to wake up.
 * 
 * We use three components to utilize the full idea:
 * 1. There is some memory area/variable, which is the _condition_ (in our case a boolean that
 *    denotes whether the measurement thread should stop measuring);
 * 2. There is a mutex lock protecting the memory area (locks are basic synchronization primitives.
 *    At any one time, only one thread can "hold" a lock - any others attempting have to wait for
 *    the first one to release it. This ensures that only the thread holding the lock can access
 *    the shared memory from #1); and
 * 3. There is the conditional variable itself that we use to notify the other thread an update has
 *    occurred.
 * 
 * The procedure is as follows:
 * - Any thread wishing to interact with the semaphore must acquire the mutex lock first to get
 *   access to the condition.
 * - Then, it depends on whether the thread is updating and sending a notification - or waiting for
 *   an update:
 *    - _Sending:_ The sending thread, while holding the lock, updates the condition. Then it
 *      notifies an update has been done.
 *    - _Receiving:_ The receiving thread will _block_ until a notification is received. Everytime
 *      a notification is received, it has to check the condition to see if the waiting is done. As
 *      such, it will _start_ with a lock on the condition; _release_ it while waiting; and then
 *      _re-lock_ it to check the condition. Then, either the wait function quits (and we have the
 *      lock still), or it continues waiting and releases the lock to allow the sending thread to
 *      update the variable.
 * 
 * It's confusing, but bear in mind that the lock and condition work together: at any one point the
 * condition is accessed, the lock must be held. The conditional variable is then a layer atop that.
 */
class Semaphore {
private:
    /* The boolean keeping track of whether the thread should stop. */
    bool should_stop;
    /* The mutex we use for accessing the conditional variable. */
    std::mutex lock;
    /* The conditional variable we use for notifying the change. */
    std::condition_variable cv;

public:
    Semaphore():
        should_stop(false),
        lock(),
        cv()
    {}


    /* Notifies the other thread that they should stop. */
    void stop() {
        {
            // Acquire access to the memory area and update the status
            // The lock is automatically released when the scope ends
            std::lock_guard lk(this->lock);
            this->should_stop = true;
        }

        // Notify the measurement thread
        this->cv.notify_all();
    }

    /* Waits for a signal during the specified timeout.
        * 
        * # Arguments
        * - `duration`: Some duration to wait for maximally.
        * 
        * # Returns
        * Whether to stop (true) or not (false).
        */
    template<typename T, typename U> bool wait_for_stop_signal(std::chrono::duration<T, U> duration) {
        // We start the timer
        std::chrono::time_point end = std::chrono::system_clock::now() + duration;

        // Note: `unique_lock` is like `lock_guard` above, except it has unlock- and relock-functions
        // that the conditional variable requires
        // We shall now wait to be notified until either we hit timeout, or we should stop
        std::unique_lock lk(this->lock);
        while (this->cv.wait_until(lk, end) != std::cv_status::timeout && !this->should_stop) {}

        // Now we've either timeouted (this->should_stop is false) or we should stop
        return this->should_stop;
    }
};



/* Represents a single measurement of energy in the background. */
class EnergyMeasurement {
private:
    /* The thread that does the measurement in the background. May be NULL. */
    std::thread* thread;
    /* The results collected during measuring. */
    std::vector<std::pair<uint64_t, int64_t>>* results;
    /* The flag telling the thread to continue. */
    Semaphore* semaphore;

public:
    /* Constructor for the EnergyMeasurement.
     * 
     * # Arguments
     * - `measurement_file`: The input path where the measurement is taken from.
     */
    EnergyMeasurement(const std::string& measurement_file);
    /* Copy constructor for the EnergyMeasurement, which doesn't exist. */
    EnergyMeasurement(const EnergyMeasurement&) = delete;
    /* Move constructor for the EnergyMeasurement. */
    EnergyMeasurement(EnergyMeasurement&&);
    /* Destructor for the EnergyMeasurement. */
    ~EnergyMeasurement();



    /* Constructor for the EnergyMeasurement.
     * 
     * # Arguments
     * - `measurement_file`: The input path where the measurement is taken from.
     * 
     * # Returns
     * A new EnergyMeasurement that will measure as long as it exists. Call
     * `EnergyMeasurement::stop()` to stop it.
     */
    inline static EnergyMeasurement start(const std::string& measurement_file) { return EnergyMeasurement(measurement_file); }

    /* Stops the measurement.
     * 
     * This will block the current thread until one more result is read. This ensures that there
     * are always at least two measurements.
     * 
     * Note that the object is left in a legal, but unusable state. It won't measure anything
     * anymore.
     * 
     * # Returns
     * The list of results as obtained during the measurement.
     */
    std::vector<std::pair<uint64_t, int64_t>> stop();



    /* Copy assignment operator for the EnergyMeasurement, which doesn't exist. */
    EnergyMeasurement& operator=(const EnergyMeasurement&) = delete;
    /* Move assignment operator for the EnergyMeasurement. */
    inline EnergyMeasurement& operator=(EnergyMeasurement&& other) { if (this != &other) { swap(*this, other); } return *this; }
    /* Swap operator for the EnergyMeasurement class. */
    friend void swap(EnergyMeasurement& em1, EnergyMeasurement& em2) {
        using std::swap;

        swap(em1.thread, em2.thread);
        swap(em1.results, em2.results);
        swap(em1.semaphore, em2.semaphore);
    }
};

#endif
