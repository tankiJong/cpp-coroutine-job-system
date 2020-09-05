#pragma once
#include <queue>
#include <shared_mutex>
#include <span>

template< typename T >
class LockQueue
{
public:
   using size_type = typename std::queue<T>::size_type;
   size_type Enqueue( const T& ele )
   {
      std::scoped_lock lock( mAccessLock );
      size_t index = mItems.size();
      mItems.push_back( ele );
      return index;
   }

   bool Dequeue( T& outEle )
   {
      std::scoped_lock lock( mAccessLock );
      if( mItems.empty() ) return false;

      outEle = std::move( mItems.front() );
      mItems.pop_front();
      return true;
   }

   size_type Count() const
   {
      mAccessLock.lock_shared();
      size_type size = mItems.size();
      mAccessLock.unlock_shared();
      return size;
   }

   void FlushAndClear(std::vector<T>& container)
   {
      std::scoped_lock lock( mAccessLock );
      container.insert( container.end(), mItems.begin(), mItems.end() );
      mItems.clear();
   }

   void Lock()
   {
      mAccessLock.lock();
   }

   void Unlock()
   {
      mAccessLock.unlock();
   }

protected:
   std::deque<T> mItems;
   mutable std::shared_mutex mAccessLock;
};

template< typename T >
class ClosableLockQueue
{
public:
   bool Enqueue( const T& ele )
   {
      std::scoped_lock lock( mAccessLock );
      if(mIsClosed)
         return false;
      mItems.push_back( ele );
      return true;
   }

   bool Enqueue( std::span<T> eles )
   {
      std::scoped_lock lock( mAccessLock );
      if(mIsClosed)
         return false;
      mItems.insert( mItems.end(), eles.begin(), eles.end() );
      return true;
   }

   void Dequeue( T& outEle )
   {
      std::scoped_lock lock( mAccessLock );
      ASSERT_DIE( !mItems.empty() );
      outEle = std::move( mItems.front() );
      mItems.pop_front();
   }

   void CloseAndFlush( std::vector<T>& container )
   {
      Close();
      container.insert( container.end(), mItems.begin(), mItems.end() );
   }

   void Close()
   {
      std::scoped_lock lock( mAccessLock );
      mIsClosed = true;
   }

   bool IsClosed() const
   {
      std::scoped_lock lock( mAccessLock );
      return mIsClosed;
   }

   size_t Count() const
   {
      std::scoped_lock lock( mAccessLock );
      return mItems.size();
   }

protected:
   std::deque<T>      mItems    = {};
   bool               mIsClosed = false;
   mutable std::mutex mAccessLock;
};
