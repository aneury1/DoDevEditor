package Backend;

import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.io.File;

import javax.swing.JOptionPane;

public class SaveAndClose extends WindowAdapter {

	FrontEnd.DoDevEditor editor;

	public SaveAndClose(FrontEnd.DoDevEditor editor) {
		this.editor = editor;
	}

	public void windowClosing(java.awt.event.WindowEvent arg0){
	    ///
		int res;
	    if(editor == null)
	    {
	         System.exit(1);
	    }
	    else
	    {
	    	if(editor.isFileOpen() == true && editor.getFilename() != null)
	    	{
	    		String ask_string = "Save the "+  editor.getFilename() + ", and Exit ? ";
	    		res = JOptionPane.showConfirmDialog(null,ask_string  , "EXIT !!", JOptionPane.YES_NO_OPTION);
				if(res == 0)
				{
					boolean fd = Backend.OpenAndSaveTextFile.SaveFile(editor.getFileArchive(), editor.getText());
				    if(fd == false)
				    	return;
				
				}
	    	
	    	
	    	}
	    	else if(editor.isFileOpen() == false || editor.getText().length() > 0){
	    		String ask_string = "Save change in Untitled file, and Exit ? ";
	    		 res = JOptionPane.showConfirmDialog(null,ask_string  , "EXIT !!", JOptionPane.YES_NO_OPTION);
					if(res == 0)
					{
						File  fd =   Backend.OpenAndSaveTextFile.SaveOption(null, editor.getText());
					    if(fd == null)
					    	return;
					}
	    		 
	    	}
	    	else
	    	{
	    		System.exit(1);
	    	}
	    	System.exit(0);
	    }
	    
		
		
		
	}

}
