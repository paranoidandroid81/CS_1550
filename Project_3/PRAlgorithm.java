/*
 *  Class for page replacement algorithms in simulator
 *  CS1550: Project 3
 *  Author: Michael Korst
 */

  import java.io.IOException;
  import java.io.*;
  import java.io.IOException;
  import java.io.PrintWriter;
  import java.util.*;
  import java.lang.*;

 public class PRAlgorithm
 {
   private int num_frames;                                 //number of frames in sim
   private String alg_type = "";                               //algorithm name
   private String tracefile = "";                               //sim trace file
   private int mem_accesses = 0;                             //stores # of mem accesses for alg
   private int page_faults = 0;                             //# of page faults for this alg
   private int disk_writes = 0;                               //# of disk writes for this alg
   private int refresh_rate = 0;                        //optional use in NRU

   public PRAlgorithm(String alg, String trace, int frames)
   {
     alg_type = alg;
     tracefile = trace;
     num_frames = frames;
   }

   //constructor for nru w/ refresh rate
   public PRAlgorithm(String alg, String trace, int frames, int refresh)
   {
     alg_type = alg;
     tracefile = trace;
     num_frames = frames;
     refresh_rate = refresh;
   }

   //runs appropriate algorithm for instance
   public void run_alg() throws IOException
   {
     switch(alg_type)
     {
       case "opt":
         optimal();
         break;
       case "clock":
         clock();
         break;
       case "fifo":
         fifo();
         break;
       case "nru":
         nru();
         break;
     }
   }

   //prints statistics for algorithm in specified format
   public void print_stats()
   {
     String alg_print = "";
     switch(alg_type)
     {
       case "opt":
         alg_print = "Opt";
         break;
       case "clock":
         alg_print = "Clock";
         break;
       case "fifo":
         alg_print = "FIFO";
         break;
       case "nru":
         alg_print = "NRU";
         break;
     }
     System.out.println("Algorithm:\t" + alg_print);
     System.out.println("Number of frames:\t" + num_frames);
     System.out.println("Total memory accesses:\t" + mem_accesses);
     System.out.println("Total page faults:\t" + page_faults);
     System.out.println("Total writes to disk:\t" + disk_writes);
   }

   //prints page fault stats to csv file for efficient graphing
   public void print_csv() throws IOException
   {
     StringBuilder sb = new StringBuilder();
     sb.append(tracefile);
     sb.append("-");
     sb.append(alg_type);
     sb.append(".csv");
     PrintWriter out_csv = new PrintWriter(new FileWriter(sb.toString(), true));
     out_csv.write(new Integer(page_faults).toString());
     out_csv.close();
   }

   //begin algorithm methods
   //implementation of optimal algorithm (not feasible in practice)
   //replace page used farthest in future (assumes future knowledge)
   private void optimal() throws IOException
   {
     BufferedReader br;
     HashMap<Integer, List<Integer>> access_order;              //map each page in table to list of access orders when processing for optimal
     HashMap<Integer, Integer> page_to_entry = new HashMap<Integer, Integer>();         //maps pages to index in memory
     List<PTE> phys_mem = new ArrayList<PTE>(num_frames);              //stores physical frames in list format
     access_order = new HashMap<Integer, List<Integer>>();
     br = new BufferedReader(new FileReader(tracefile));
     String line = br.readLine();
     int curr_page;
     int sequence = 0;              //sequence of instructions, use to determine priorities in replacement
     //begin 1st time around on file, get sequences for optimization second time around
     while (line != null)
     {
       //parse page to access in hex
       String mem_string = "0x" + line.substring(0, 5);         //first 4 hex = page #
       long mem_addr = Long.decode(mem_string);             //parse hex string to decimal
       curr_page = (int)(mem_addr);
       //check if access list for this page hasn't been initialized yet, do it if so
       if (!access_order.containsKey(curr_page))
       {
         access_order.put(curr_page, new ArrayList<Integer>());
       }
       List<Integer> next_list = access_order.get(curr_page);
       next_list.add(sequence);           //add memory access for page for optimization
       access_order.put(curr_page, next_list);          //replace sequence for page with updated sequence
       sequence++;
       line = br.readLine();
     }
     try
     {
       br = new BufferedReader(new FileReader(tracefile));
     } catch (IOException e)
     {
       System.err.println(e);
       System.exit(1);        //something wrong with input file, exit
     }
     line = br.readLine();
     //begin 2nd time thru file, use stored sequences to optimize page replacement
     char mode;               //read or write?
     int frames_loaded = 0;           //track how many phys frames loaded into memory, max = num_frames
     while (line != null)
     {
       //parse page to access in hex
       String mem_string = "0x" + line.substring(0, 5);         //first 5 hex = page #
       long mem_addr = Long.decode(mem_string);             //parse hex string to decimal
       curr_page = (int)(mem_addr);
       mode = line.charAt(9);                         //parse R or W access
       mem_accesses++;                              //increment for each memory access (line in file)
       //begin logic for determining if page fault, if evict, if dirty
       //room to load into physical memory!
       if (frames_loaded < num_frames)
       {
         access_order.get(curr_page).remove(0);             //get rid of 1st memory access of address as accessed already, use remainder to optimize
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_mem.get(curr_frame).setDirty(true);
           }
         } else
         {
           //page fault, space for new page though
           System.out.println(mem_string + ": page fault - no eviction");
           page_to_entry.put(curr_page, frames_loaded);
           phys_mem.add(frames_loaded, new PTE(curr_page));              //load current page into empty frame
           phys_mem.get(frames_loaded).setRef(true);               //referenced and valid
           phys_mem.get(frames_loaded).setValid(true);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_mem.get(curr_frame).setDirty(true);
           }
           page_faults++;
           frames_loaded++;
         }
       } else                 //no empty frames
       {
         access_order.get(curr_page).remove(0);             //get rid of 1st memory access of address as accessed now, use remainder to optimize
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_mem.get(curr_frame).setDirty(true);
           }
         } else           //eviction time, evict frame used furthest in the future
         {
           page_faults++;           //need to load into memory, fault
           int to_evict = 0;              //start at frame index 0, find which to evict
           int farthest = 0;              //for seeing which page should be removed
           //iterate thru frames, find one used furthest in the future
           for (PTE p : phys_mem)
           {
             int candidate_page = p.getPageNum();           //see how far in future this page accessed
             if (access_order.get(candidate_page).isEmpty())
             {
               //no accesses, evict this page!
               to_evict = page_to_entry.get(candidate_page);
               break;
             }
             int next_use = access_order.get(candidate_page).get(0);          //first reference of current frame, check if oldest
             farthest = access_order.get(phys_mem.get(to_evict).getPageNum()).get(0);      //find farthest used reference of current possible eviction frame
             //now compare current candidate, if used farther in future, make it top candidate
             if (next_use > farthest)
             {
               to_evict = page_to_entry.get(candidate_page);
             }
           }
           //now we've found frame we want to evict, need to see if disk write needed
           if (phys_mem.get(to_evict).getDirty())
           {
             disk_writes++;                     //need to write to disk as modified
             System.out.println(mem_string + ": page fault - evict dirty");             //output
           } else
           {
             //no write needed
             System.out.println(mem_string + ": page fault - evict clean");
           }
           //remove old mapping for removed page
           int remove_page = phys_mem.get(to_evict).getPageNum();
           page_to_entry.remove(remove_page);
           //replace spot in phys memory with new page
           phys_mem.set(to_evict, new PTE(curr_page));
           //add mapping of page to index
           page_to_entry.put(curr_page, to_evict);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             phys_mem.get(to_evict).setDirty(true);
           }
           phys_mem.get(to_evict).setRef(true);        //referenced
           phys_mem.get(to_evict).setValid(true);        //valid
         }
       }
       line = br.readLine();                  //read next line
     }
   }

   //implementation of clock algorithm
   //use circular queue to pick evicted page
   private void clock() throws IOException
   {
     BufferedReader br;
     HashMap<Integer, Integer> page_to_entry = new HashMap<Integer, Integer>();         //maps pages to index in memory
     List<PTE> phys_mem = new ArrayList<PTE>(num_frames);              //stores physical frames in list format
     int clock_hand = 0;                         //"clock hand" to find evicted, start at index 0
     boolean found = false;                       //use later when looking for page to evict
     br = new BufferedReader(new FileReader(tracefile));
     String line = br.readLine();
     int curr_page;
     //begin reading thru memory accesses, loading into memory based on clock algorithm
     char mode;               //read or write?
     int frames_loaded = 0;           //track how many phys frames loaded into memory, max = num_frames
     while (line != null)
     {
       //parse page to access in hex
       String mem_string = "0x" + line.substring(0, 5);         //first 5 hex = page #
       long mem_addr = Long.decode(mem_string);             //parse hex string to decimal
       curr_page = (int)(mem_addr);
       mode = line.charAt(9);                         //parse R or W access
       mem_accesses++;                              //increment for each memory access (line in file)
       //begin logic for determining if page fault, if evict, if dirty
       //room to load into physical memory!
       if (frames_loaded < num_frames)
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           int curr_frame = page_to_entry.get(curr_page);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             phys_mem.get(curr_frame).setDirty(true);
           }
           phys_mem.get(curr_frame).setRef(true);             //referenced!
         } else
         {
           //page fault, space for new page though
           System.out.println(mem_string + ": page fault - no eviction");
           page_to_entry.put(curr_page, frames_loaded);
           phys_mem.add(frames_loaded, new PTE(curr_page));              //load current page into empty frame
           phys_mem.get(frames_loaded).setRef(true);               //referenced and valid
           phys_mem.get(frames_loaded).setValid(true);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_mem.get(curr_frame).setDirty(true);
           }
           page_faults++;
           frames_loaded++;
         }
       } else                 //no empty frames
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           int curr_frame = page_to_entry.get(curr_page);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             phys_mem.get(curr_frame).setDirty(true);
           }
           phys_mem.get(curr_frame).setRef(true);             //referenced!
         } else           //eviction time, evict first frame in circular queue where R==0
         {
           page_faults++;           //need to load into memory, fault
           int to_evict = 0;              //which page we will evict from physical
           found = false;               //need to look for page to evict, haven't found yet
           //iterate thru frames in circular queue, if R==0, evict, if R==1, set to 0 + advance
           while (!found)
           {
             if(!phys_mem.get(clock_hand).getRef())
             {
               found = true;            //ref == 0, we can evict!
               to_evict = clock_hand;         //save num of evicted
             } else {
               phys_mem.get(clock_hand).setRef(false);        //ref == 1, move to 0 and try next
             }
             clock_hand = (clock_hand + 1) % num_frames;               //advance clock hand within circular queue of frames
           }
           //now we've found frame we want to evict, need to see if disk write needed
           if (phys_mem.get(to_evict).getDirty())
           {
             disk_writes++;                     //need to write to disk as modified
             System.out.println(mem_string + ": page fault - evict dirty");             //output
           } else
           {
             //no write needed
             System.out.println(mem_string + ": page fault - evict clean");
           }
           //remove old mapping for removed page
           int remove_page = phys_mem.get(to_evict).getPageNum();
           page_to_entry.remove(remove_page);
           //replace spot in phys memory with new page
           phys_mem.set(to_evict, new PTE(curr_page));
           //add mapping of page to index
           page_to_entry.put(curr_page, to_evict);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             phys_mem.get(to_evict).setDirty(true);
           }
           phys_mem.get(to_evict).setRef(true);               //referenced
           phys_mem.get(to_evict).setValid(true);               //valid
         }
       }
       line = br.readLine();                  //read next line
     }
   }

   //implementation of fifo algorithm
   private void fifo() throws IOException
   {
     BufferedReader br;
     PTE[] phys_mem = new PTE[num_frames];              //placeholder array for queue creation
     FixedLengthQueue phys_flq = new FixedLengthQueue(phys_mem);        //fifo queue for frames
     HashMap<Integer, Integer> page_to_entry = new HashMap<Integer, Integer>();         //maps pages to index in memory
     br = new BufferedReader(new FileReader(tracefile));
     String line = br.readLine();
     int curr_page;
     //begin reading thru memory accesses, loading into memory based on clock algorithm
     char mode;               //read or write?
     int frames_loaded = 0;           //track how many phys frames loaded into memory, max = num_frames
     while (line != null)
     {
       //parse page to access in hex
       String mem_string = "0x" + line.substring(0, 5);         //first 5 hex = page #
       long mem_addr = Long.decode(mem_string);             //parse hex string to decimal
       curr_page = (int)(mem_addr);
       mode = line.charAt(9);                         //parse R or W access
       mem_accesses++;                              //increment for each memory access (line in file)
       //begin logic for determining if page fault, if evict, if dirty
       //room to load into physical memory!
       if (frames_loaded < num_frames)
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_flq.get(curr_frame).setDirty(true);
           }
         } else
         {
           //page fault, space for new page though
           System.out.println(mem_string + ": page fault - no eviction");
           phys_flq.add(new PTE(curr_page));              //load current page into empty frame
           int new_idx = phys_flq.get_index(new PTE(curr_page));        //get index, add mapping of page to frame index
           page_to_entry.put(curr_page, new_idx);
           phys_flq.get(new_idx).setRef(true);               //referenced and valid
           phys_flq.get(new_idx).setValid(true);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_flq.get(curr_frame).setDirty(true);
           }
           page_faults++;
           frames_loaded++;
         }
       } else                 //no empty frames
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_flq.get(curr_frame).setDirty(true);
           }
         } else           //eviction time, evict oldest in queue
         {
           page_faults++;           //need to load into memory, fault
           //get head of fifo queue
           PTE evict_frame = phys_flq.peek();
           //save page num of evicted
           int evict_page = evict_frame.getPageNum();
           //now we've found frame we want to evict, need to see if disk write needed
           if (evict_frame.getDirty())
           {
             disk_writes++;                     //need to write to disk as modified
             System.out.println(mem_string + ": page fault - evict dirty");             //output
           } else
           {
             //no write needed
             System.out.println(mem_string + ": page fault - evict clean");
           }
           //remove mapping for evicted page
           page_to_entry.remove(evict_page);
           //replace spot in phys memory with new page
           phys_flq.add(new PTE(curr_page));
           int new_idx = phys_flq.get_index(new PTE(curr_page));        //get index, add mapping of page to frame index
           page_to_entry.put(curr_page, new_idx);
           phys_flq.get(new_idx).setRef(true);               //referenced and valid
           phys_flq.get(new_idx).setValid(true);
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             int curr_frame = page_to_entry.get(curr_page);
             phys_flq.get(curr_frame).setDirty(true);
           }
         }
       }
       line = br.readLine();                  //read next line
     }
   }

   //implementation of not recently used (nru) algorithm
   //pick page to evict from lowest non-empty class randomly
   private void nru() throws IOException
   {
     BufferedReader br;
     Random rand_evict = new Random();                //create Random object for later choice of eviction (if necessary)
     HashMap<Integer, Integer> page_to_entry = new HashMap<Integer, Integer>();         //maps pages to index in memory
     List<PTE> phys_mem = new ArrayList<PTE>(num_frames);              //stores physical frames in list format
     HashMap<Integer, Integer> page_to_class = new HashMap<Integer, Integer>();        //maps pages to class
     //lists for all 4 classes based on ref/dirty bit
     List<PTE> c_zero = new ArrayList<PTE>(num_frames);             //not ref, not dirty
     List<PTE> c_one = new ArrayList<PTE>(num_frames);             //not ref, dirty
     List<PTE> c_two = new ArrayList<PTE>(num_frames);             //ref, not dirty
     List<PTE> c_three = new ArrayList<PTE>(num_frames);             //ref, dirty
     br = new BufferedReader(new FileReader(tracefile));
     String line = br.readLine();
     int curr_page;
     //begin reading thru memory accesses, loading into memory based on clock algorithm
     char mode;               //read or write?
     int frames_loaded = 0;           //track how many phys frames loaded into memory, max = num_frames
     while (line != null)
     {
       //parse page to access in hex
       String mem_string = "0x" + line.substring(0, 5);         //first 5 hex = page #
       long mem_addr = Long.decode(mem_string);             //parse hex string to decimal
       curr_page = (int)(mem_addr);
       mode = line.charAt(9);                         //parse R or W access
       mem_accesses++;                              //increment for each memory access (line in file)
       //check if time to refresh, set all ref bits to 0
       if (mem_accesses % refresh_rate == 0)
       {
         //must move all from c3 into c1, c2 into c0
         List<PTE> c3_temp = new ArrayList<PTE>(c_three);         //hold while transferring
         c_three.clear();           //can clear c3 while waiting for new
         for (PTE p : c3_temp)
         {
           //un-set all ref bits
           p.setRef(false);
           page_to_class.put(p.getPageNum(), 1);        //now all assigned to c1
         }
         c_one.addAll(c3_temp);               //add all the elements to c1
         List<PTE> c2_temp = new ArrayList<PTE>(c_two);
         c_two.clear();                   //can clear c2 for now
         for (PTE p : c2_temp)
         {
           //un-set all ref bits
           p.setRef(false);
           page_to_class.put(p.getPageNum(), 0);        //now all assigned to c0
         }
         c_zero.addAll(c2_temp);
       }
       //begin logic for determining if page fault, if evict, if dirty
       //room to load into physical memory!
       if (frames_loaded < num_frames)
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame]
           int curr_frame = page_to_entry.get(curr_page);
           int curr_class = page_to_class.get(curr_page);           //store class for checking later
           PTE curr = phys_mem.get(curr_frame);                 //current PTE, in case needs changing
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             //check if right class, should be c3, otherwise must move!
             if (curr_class != 3)
             {
               //c2
               if (curr_class == 2)
               {
                 c_two.remove(curr);
               } else if (curr_class == 0)             //c0
               {
                 c_zero.remove(curr);
               } else             //c1
               {
                 c_one.remove(curr);
               }
               c_three.add(curr);         //add to c3, mapping
               page_to_class.put(curr_page, 3);
             }
             curr.setDirty(true);         //now can set dirty bit
           }
           curr_class = page_to_class.get(curr_page);         //check again, if not dirty, may need to switch classes
           //if in c0 or c1 (not ref), need to switch to a class with ref bit
           if (curr_class == 0 || curr_class == 1)
           {
             if (curr_class == 0)         //c0 -> c2
             {
               c_zero.remove(curr);
               c_two.add(curr);
               page_to_class.put(curr_page, 2);
             } else                 //c1 -> c3
             {
               c_one.remove(curr);
               c_three.add(curr);
               page_to_class.put(curr_page, 3);
             }
           }
           curr.setRef(true);                             //ref might be 0, should be made 1 as referenced
         } else
         {
           //page fault, space for new page though
           System.out.println(mem_string + ": page fault - no eviction");
           page_to_entry.put(curr_page, frames_loaded);
           phys_mem.add(frames_loaded, new PTE(curr_page));              //load current page into empty frame
           phys_mem.get(frames_loaded).setRef(true);               //referenced and valid
           phys_mem.get(frames_loaded).setValid(true);
           PTE curr = phys_mem.get(frames_loaded);              //for putting in classes
           //if write, now must set dirty bit for frame, add to c3
           if (mode == 'W')
           {
             curr.setDirty(true);
             c_three.add(curr);
             page_to_class.put(curr_page, 3);
           } else       //put in c2, not dirty
           {
             c_two.add(curr);
             page_to_class.put(curr_page, 2);
           }
           page_faults++;
           frames_loaded++;
         }
       } else                 //no empty frames
       {
         //already loaded into frame!
         if (page_to_entry.containsKey(curr_page))
         {
           System.out.println(mem_string + ": hit");                 //was in frame]
           int curr_frame = page_to_entry.get(curr_page);
           int curr_class = page_to_class.get(curr_page);           //store class for checking later
           PTE curr = phys_mem.get(curr_frame);                 //current PTE, in case needs changing
           //if write, now must set dirty bit for frame
           if (mode == 'W')
           {
             //check if right class, should be c3, otherwise must move!
             if (curr_class != 3)
             {
               //c2
               if (curr_class == 2)
               {
                 c_two.remove(curr);
               } else if (curr_class == 0)             //c0
               {
                 c_zero.remove(curr);
               } else             //c1
               {
                 c_one.remove(curr);
               }
               c_three.add(curr);         //add to c3, mapping
               page_to_class.put(curr_page, 3);
             }
             curr.setDirty(true);         //now can set dirty bit
           }
           curr_class = page_to_class.get(curr_page);         //check again, if not dirty, may need to switch classes
           //if in c0 or c1 (not ref), need to switch to a class with ref bit
           if (curr_class == 0 || curr_class == 1)
           {
             if (curr_class == 0)         //c0 -> c2
             {
               c_zero.remove(curr);
               c_two.add(curr);
               page_to_class.put(curr_page, 2);
             } else                 //c1 -> c3
             {
               c_one.remove(curr);
               c_three.add(curr);
               page_to_class.put(curr_page, 3);
             }
           }
           curr.setRef(true);                             //ref might be 0, should be made 1 as referenced
         } else           //eviction time, evict random frame from lowest non-empty class
         {
           page_faults++;           //need to load into memory, fault
           int to_evict = 0;              //which page we will evict from physical
           int class_idx = 0;             //index within class to evict, chosen randomly
           PTE evict_entry;                //holds which PTE to evict
           //TODO: Implement nru algorithm for eviction here
           //find lowest non-empty class to evict page
           if (!c_zero.isEmpty())
           {
             class_idx = rand_evict.nextInt(c_zero.size());         //choose random index between 0 and size - 1
             evict_entry = c_zero.get(class_idx);
             c_zero.remove(class_idx);            //get rid of the evicted from class list
           } else if (!c_one.isEmpty())
           {
             class_idx = rand_evict.nextInt(c_one.size());         //choose random index between 0 and size - 1
             evict_entry = c_one.get(class_idx);
             c_one.remove(class_idx);            //get rid of the evicted from class list
           } else if (!c_two.isEmpty())
           {
             class_idx = rand_evict.nextInt(c_two.size());         //choose random index between 0 and size - 1
             evict_entry = c_two.get(class_idx);
             c_two.remove(class_idx);            //get rid of the evicted from class list
           } else
           {
             class_idx = rand_evict.nextInt(c_three.size());         //choose random index between 0 and size - 1
             evict_entry = c_three.get(class_idx);
             c_three.remove(class_idx);            //get rid of the evicted from class list
           }
           to_evict = page_to_entry.get(evict_entry.getPageNum());            //store phys index of evicted
           //now we've found frame we want to evict, need to see if disk write needed
           if (phys_mem.get(to_evict).getDirty())
           {
             disk_writes++;                     //need to write to disk as modified
             System.out.println(mem_string + ": page fault - evict dirty");             //output
           } else
           {
             //no write needed
             System.out.println(mem_string + ": page fault - evict clean");
           }
           //remove old mapping for removed page
           int remove_page = evict_entry.getPageNum();
           page_to_entry.remove(remove_page);
           //replace spot in phys memory with new page
           phys_mem.set(to_evict, new PTE(curr_page));
           //add mapping of page to index
           page_to_entry.put(curr_page, to_evict);
           //if write, now must set dirty bit for frame, c3
           if (mode == 'W')
           {
             phys_mem.get(to_evict).setDirty(true);
             c_three.add(phys_mem.get(to_evict));
             page_to_class.put(curr_page, 3);
           } else       //put in c2, not dirty
           {
             c_two.add(phys_mem.get(to_evict));
             page_to_class.put(curr_page, 2);
           }
           phys_mem.get(to_evict).setRef(true);               //referenced
           phys_mem.get(to_evict).setValid(true);               //valid
         }
       }
       line = br.readLine();                  //read next line
     }
   }
 }
