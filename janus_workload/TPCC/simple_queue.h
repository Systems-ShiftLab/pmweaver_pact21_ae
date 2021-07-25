/*
  Author: Vaibhav Gogte <vgogte@umich.edu>
          Aasheesh Kolli <akolli@umich.edu>
*/

//#include <iostream>
#define QUEUE_SIZE 20

class simple_queue {
  private:
    long entries[QUEUE_SIZE];
    long head;
    long tail;
  public:
    simple_queue() {
      head = 0;
      tail = 0;
    }

    ~simple_queue() {}

    bool empty() {
      return (head == tail);
    }

    bool full() {
      if(tail == 0)
        return (head == QUEUE_SIZE-1);
      return (head == tail-1);
    }

    int size() {
      if(head >= tail) {
        return head - tail;
      }
      else {
        return (QUEUE_SIZE - tail + head);
      }
    }

    bool push(long entry) {
      if(full())
        return false;
      entries[head] = entry;
      if(head == QUEUE_SIZE-1)
        head = 0;
      else
        head++;
      return true;
    }

    long front() {
      return entries[tail];
    }

    bool pop() {
      if(empty())
        return false;
      if(tail == QUEUE_SIZE-1)
        tail = 0;
      else
        tail++;
      return true;
    }

    //void printQueue() {
    //  std::cout<<"head tail "<<head<<" "<<tail<<std::endl;
    //  for(int i=0; i<QUEUE_SIZE; i++) {
    //    std::cout<<i<<" "<<entries[i]<<std::endl;
    //  }
    //}
};
