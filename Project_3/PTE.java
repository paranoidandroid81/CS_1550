/*
 *  Class for page table entry
 *  CS1550: Project 3
 *  Author: Michael Korst
 */

 public class PTE
 {
   private boolean valid_bit;       //is entry valid?
   private boolean dirty_bit;       //has entry been modified?
   private boolean ref_bit;         //has entry been referenced?
   private int page_num;            //page number

   //constructor for page table entry
   public PTE(int pn)
   {
     page_num = pn;
     valid_bit = false;
     dirty_bit = false;
     ref_bit = false;
   }

   //begin setters and getters for private data members of PTE

   public boolean getValid()
   {
     return valid_bit;
   }

   public int getPageNum()
   {
     return page_num;
   }

   public boolean getDirty()
   {
     return dirty_bit;
   }

   public boolean getRef()
   {
     return ref_bit;
   }

   public void setValid(boolean _valid)
   {
     valid_bit = _valid;
   }

   public void setDirty(boolean _dirty)
   {
     dirty_bit = _dirty;
   }

   public void setRef(boolean _ref)
   {
     ref_bit = _ref;
   }

   public void setPageNum(int _pn)
   {
     page_num = _pn;
   }

   //see if 2 PTEs are the same
   public boolean equals(PTE other)
   {
     return this.valid_bit == other.getValid() && this.dirty_bit == other.getDirty()
     && this.ref_bit == other.getRef() && this.page_num == other.getPageNum();
   }

 }
