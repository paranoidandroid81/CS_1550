/*Implementation of fixed length queue for use in fifo
* Uses circular queue where head is removed when full
* Increments index to next oldest as remove
* CS1550: Project 3
* Author: Michael Korst
*/

import java.util.*;
import java.lang.*;

public class FixedLengthQueue implements Queue<PTE>
{
  private PTE[] frames;
  private int index;

  public FixedLengthQueue(PTE[] initial_vals)
  {
    frames = initial_vals;              //save references in the queue
    //oldest element should be replaced when queue is full
    index = 0;
  }

  @Override
  public boolean add(PTE new_frame)
  {
    return offer(new_frame);
  }

  @Override
  public PTE element()
  {
    return frames[getHeadIndex()];
  }

  @Override
  public boolean offer(PTE new_frame)
  {
    //replace oldest with new, increment head ptr
    PTE oldest_frame = frames[index];
    frames[index] = new_frame;
    incrIndex();
    return true;
  }

  @Override
  public PTE peek()
  {
    return frames[getHeadIndex()];
  }

  @Override
  public PTE poll()
  {
    throw new IllegalStateException("Poll method not available for FixedLengthQueue");
  }

  @Override
  public PTE remove()
  {
    throw new IllegalStateException("Remove method not available for FixedLengthQueue");
  }

  @Override
  public void clear()
  {
    throw new UnsupportedOperationException("Clear method not available for FixedLengthQueue");
  }

  @Override
  public boolean retainAll(Collection<?> c)
  {
    throw new UnsupportedOperationException("RetainAll method not available for FixedLengthQueue");
  }

  @Override
  public boolean removeAll(Collection<?> c)
  {
    throw new UnsupportedOperationException("RemoveAll method not available for FixedLengthQueue");
  }

  @Override
  public boolean addAll(Collection<? extends PTE> c)
  {
    throw new UnsupportedOperationException("AddAll method not available for FixedLengthQueue");
  }

  @Override
  public boolean containsAll(Collection<?> c)
  {
    throw new UnsupportedOperationException("ContainsAll method not available for FixedLengthQueue");
  }

  @Override
  public boolean remove(Object o)
  {
    throw new UnsupportedOperationException("Remove method not available for FixedLengthQueue");
  }

  @Override
  public PTE[] toArray()
  {
    return frames;
  }

  @Override
  public <PTE>PTE[] toArray(PTE[] a)
  {
    throw new UnsupportedOperationException("ToArray method not available for FixedLengthQueue");
  }

  @Override
  public Iterator<PTE> iterator()
  {
    Iterator<PTE> it = new Iterator<PTE>()
    {
      private int curr_idx = 0;

      @Override
      public boolean hasNext()
      {
        return curr_idx < frames.length && frames[curr_idx] != null;
      }

      @Override
      public PTE next()
      {
        return frames[curr_idx++];
      }

      @Override
      public void remove()
      {
        throw new UnsupportedOperationException();
      }
    };
    return it;
  }

  @Override
  public boolean contains(Object o)
  {
    for (PTE p : frames)
    {
      if (p.equals(o))
      {
        return true;
      }
    }
    return false;
  }

  public int get_index(PTE to_find)
  {
    int i = 0;
    for (PTE p : frames)
    {
      if (p.equals(to_find))
      {
        return i;
      }
      i++;
    }
    return -1;              //shouldn't happen...
  }

  @Override
  public boolean isEmpty()
  {
    if (frames.length > 0)
    {
      return false;
    } else {
      return true;
    }
  }

  @Override
  public int size()
  {
    return frames.length;
  }

  public PTE get(int get_idx) throws IndexOutOfBoundsException
  {
    if (get_idx >= frames.length)
    {
      throw new IndexOutOfBoundsException("Invalid index: " + get_idx);
    }
    if (get_idx >= frames.length)
    {
      get_idx -= frames.length;
    }
    return frames[get_idx];
  }

  private void incrIndex()
  {
    index = nextIndex(index);
  }

  private int nextIndex(int curr_idx)
  {
    //if past end, go back to beginning, circular
    if (curr_idx + 1 >= frames.length)
    {
      return 0;
    } else
    {
      return curr_idx + 1;
    }
  }

  private int previousIndex(int curr_idx)
  {
    if ((curr_idx - 1) < 0)
    {
      return frames.length - 1;
    } else
    {
      return curr_idx - 1;
    }
  }

  //get index of next to remove
  private int getHeadIndex()
  {
    return index;
  }
}
