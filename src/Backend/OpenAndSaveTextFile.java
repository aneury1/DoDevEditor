package Backend;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;

import javax.print.*;
import javax.print.DocPrintJob;
import javax.print.PrintService;
import javax.print.PrintServiceLookup;
import javax.print.attribute.HashPrintRequestAttributeSet;
import javax.print.attribute.PrintRequestAttributeSet;
import javax.print.attribute.standard.Copies;
import javax.print.event.PrintJobAdapter;
import javax.print.event.PrintJobEvent;
import javax.swing.JComponent;
import javax.swing.JFileChooser;
import javax.swing.JOptionPane;

public class OpenAndSaveTextFile {
	static boolean ok;
	
	
	public static  String readFile(String path)
	{
		 
		File selectedFile = new File(path);
		  String ret = null;
		  BufferedReader br = null;
		try{
		  StringBuilder builder = new StringBuilder();
		  br = new BufferedReader(new FileReader(selectedFile.getAbsolutePath()));
	      StringBuilder sb = new StringBuilder();
	      String line;
		  line = br.readLine();
				    while (line != null) {
	        	        sb.append(line);
	        	        sb.append(System.lineSeparator());
	        	        line = br.readLine();
	        	    }
	        	    ret = sb.toString();
	        	
		}catch(Exception e ){
			
		}
		finally{
	    try {
			br.close();
		} catch (IOException e) {
			 
			e.printStackTrace();
		}finally{
		return ret;
		}
		}
	}
	
	
	
	
	
	
	
	
	
	
	public static String ReadFileByPanel(JComponent comp){
		String ret = null;
		JFileChooser fileChooser = new JFileChooser();
		fileChooser.setCurrentDirectory(new File(System.getProperty("user.home")));
		int result = fileChooser.showOpenDialog(comp);
		if (result == JFileChooser.APPROVE_OPTION) {
		    File selectedFile = fileChooser.getSelectedFile();
		    StringBuilder builder = new StringBuilder();
		   //// System.out.println("Selected file: " + selectedFile.getAbsolutePath());
	        try {
			
	        	
	        	
	        	BufferedReader br = new BufferedReader(new FileReader(selectedFile.getAbsolutePath()));
	        	try {
	        	    StringBuilder sb = new StringBuilder();
	        	    String line;
				 
						line = br.readLine();
					 

	        	    while (line != null) {
	        	        sb.append(line);
	        	        sb.append(System.lineSeparator());
	        	        line = br.readLine();
	        	    }
	        	    ret = sb.toString();
	        	} finally {
	        	    br.close();
	        	}
	        
	        
	        
	        } catch ( Exception e) {
				 
				e.printStackTrace();
			}
		
		   
		
		}
	
		return ret;
	}

	
	public static File SaveOption(JComponent parent, String text){
	   File ret = null;
		try{
		final JFileChooser fc = new JFileChooser();
		
		int returnVal = fc.showSaveDialog(parent); //parent component to JFileChooser
		
		if( fc.getSelectedFile().exists() == false)
		{
			new File(fc.getSelectedFile().toString()).createNewFile();
		}
		if (returnVal == JFileChooser.APPROVE_OPTION) { //OK button pressed by user
		        File file = fc.getSelectedFile(); //get File selected by user
		        BufferedWriter o = new BufferedWriter(new FileWriter(file)); //use its name
                o.write(text);
                o.flush();
                o.close();
                ret = file;
		}
		}catch(Exception e){
			///e.printStackTrace();
			System.out.printf("codigo de Error [%s]\n", e.getMessage());
			if(e.getMessage().equals("null"))
			{
			     return null;	
			}
			return null;
		}
	  finally{
		  return ret;
	  }
	}
	
	
	public static boolean SaveFile(File file, String text)
	{
		try{
        BufferedWriter o = new BufferedWriter(new FileWriter(file)); //use its name
        o.write(text);
        o.flush();
        o.close();
		}catch(Exception e){
			e.printStackTrace();
			return false;
		}
		return true;
	}
	
	
	
	static String defaultPrinter ;
	public static void PrintPage(String text){
		
		
		
		class PrintJobWatcher {
			  boolean done = false;

			  PrintJobWatcher(DocPrintJob job) {
			    job.addPrintJobListener(new PrintJobAdapter() {
			      public void printJobCanceled(PrintJobEvent pje) {
			        allDone();
			      }
			      public void printJobCompleted(PrintJobEvent pje) {
			        allDone();
			      }
			      public void printJobFailed(PrintJobEvent pje) {
			        allDone();
			      }
			      public void printJobNoMoreEvents(PrintJobEvent pje) {
			        allDone();
			      }
			      void allDone() {
			        synchronized (PrintJobWatcher.this) {
			          done = true;
			          
			          JOptionPane.showMessageDialog(null, "Printer Done at : " + defaultPrinter);
			          System.out.println("Printing done ...");
			          PrintJobWatcher.this.notify();
			        }
			      }
			    });
			  }
			  public synchronized void waitForDone() {
			    try {
			      while (!done) {
			        wait();
			      }
			    } catch (InterruptedException e) {
			    }
			  }
			  }
		try{
		  defaultPrinter =
			      PrintServiceLookup.lookupDefaultPrintService().getName();
			    System.out.println("Default printer: " + defaultPrinter);
			    
			    PrintService service = PrintServiceLookup.lookupDefaultPrintService();

			    // prints the famous hello world! plus a form feed
			    InputStream is = new ByteArrayInputStream(text.getBytes("UTF8"));

			    PrintRequestAttributeSet  pras = new HashPrintRequestAttributeSet();
			    pras.add(new Copies(1));

			    DocFlavor flavor = DocFlavor.INPUT_STREAM.AUTOSENSE;
			    Doc doc = new SimpleDoc(is, flavor, null);
			    DocPrintJob job = service.createPrintJob();

			    PrintJobWatcher pjw = new PrintJobWatcher(job);
			    job.print(doc, pras);
			    pjw.waitForDone();
			    is.close();
	}catch(Exception e){
		e.printStackTrace();
	}
		}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
}
