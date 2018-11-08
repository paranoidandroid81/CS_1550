/*
 *  Main class for page replacement simulator
 *  CS1550: Project 3
 *  Author: Michael Korst
 */

 import java.io.IOException;

 public class vmsim
 {
   public static void main(String[] args) throws IOException
   {
     int frames_arg, refresh_arg;       //to store arguments for frame nums, refresh rate (if applicable)
     String trace_arg = "";               //argument for tracefile
     String alg_arg = "";                 //argument for algorithm type
     PRAlgorithm alg_sim = new PRAlgorithm(null, null, 0);    //paging sim object, initialize w/garbage to avoid Java errors...
     //begin parsing args + checking validity
     if (!args[0].equals("-n"))
     {
       System.err.println("Invalid arguments! Correct format is:\n" +
       "./vmsim –n <numframes> -a <opt|clock|fifo|nru> [-r <refresh>] <tracefile");
       System.exit(1);
     }
     frames_arg = Integer.parseInt(args[1]);        //get num frames
     if (!args[2].equals("-a"))
     {
       System.err.println("Invalid arguments! Correct format is:\n" +
       "./vmsim –n <numframes> -a <opt|clock|fifo|nru> [-r <refresh>] <tracefile");
       System.exit(1);
     }
     alg_arg = args[3];           //get algorithm type
     if (alg_arg.equals("nru") && !args[4].equals("-r"))    //nru incorrect args
     {
       System.err.println("Invalid arguments! NRU requires refresh! Correct format is:\n" +
       "./vmsim –n <numframes> -a <opt|clock|fifo|nru> [-r <refresh>] <tracefile");
       System.exit(1);
     } else if (alg_arg.equals("nru"))        //nru w/correct args
     {
       refresh_arg = Integer.parseInt(args[5]);         //get refresh rate
       trace_arg = args[6];                   //tracefile in nru
       //initalize sim object for nru
       alg_sim = new PRAlgorithm(alg_arg, trace_arg, frames_arg, refresh_arg);
     } else if (!alg_arg.equals("nru") && args[4].equals("-r"))   //non-nru w/refresh, wrong!
     {
       System.err.println("Invalid arguments! Only NRU requires refresh! Correct format is:\n" +
       "./vmsim –n <numframes> -a <opt|clock|fifo|nru> [-r <refresh>] <tracefile");
       System.exit(1);
     } else               //non-nru w/correct args
     {
       trace_arg = args[4];       //tracefile in non-nru
       //initalize sim object for non-nru
       alg_sim = new PRAlgorithm(alg_arg, trace_arg, frames_arg);
     }
     //now, run specified algorithm and print out stats
     alg_sim.run_alg();
     alg_sim.print_stats();
   }
 }
