package FrontEnd;

import java.awt.Dimension;
import java.awt.Font;
import java.awt.Image;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
///import java.io.BufferedReader;
import java.io.File;
///import java.io.FileReader;
import java.io.IOException;

import javax.imageio.ImageIO;
///import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
///import javax.swing.JScrollPane;
///import javax.swing.JTextArea;
import javax.swing.KeyStroke;

import org.fife.ui.rsyntaxtextarea.RSyntaxTextArea;
import org.fife.ui.rsyntaxtextarea.SyntaxConstants;
import org.fife.ui.rtextarea.RTextScrollPane;

import Backend.MyFileTransferHandler;
 
 
 



public class DoDevEditor implements ActionListener{

	JFrame main_window;
	JMenuBar   menuBar;
	
	public static boolean isFileInBox;
	public static String  FileName;
	
	
	
	/*file option in meni*/
	JMenu  fileM;
	JMenuItem open_;
	JMenuItem new_;
	JMenuItem save_;
	JMenuItem saveas_;
	JMenuItem PrintDlg_;
	JMenuItem Exit_;
	
	/*Edit option in menu*/
	JMenu   editM;
	JMenuItem copy_;
	JMenuItem paste_;
	JMenuItem cut_;
	JMenuItem delete_;
	JMenuItem undo_;
	JMenuItem find_;
	JMenuItem findReplace_;
	
	/* build Command Configuration */
	JMenu buildM;
	JMenuItem build_;
	JMenuItem configure_build_command_;
	JMenuItem view_last_verbose_;
	JMenuItem run_;
	JMenuItem Debug_;
	JMenuItem Configure_debug_and_debug_command_;
	
	
	JMenu editorM;
	JMenuItem configure;
	
	
	
	/*About and help*/
	JMenu About_and_Help;
	JMenuItem Help_;
	JMenuItem About_;
	
	/*the editor*/
	////JScrollPane scrollPane;
	///JTextArea text;
	RSyntaxTextArea text;
	RTextScrollPane scrollPane;
	
	/*file Operation*/
	File open_file= null;
	
	
	public RSyntaxTextArea getTextArea(){
		return text;
	}
	
	
	public boolean isFileOpen(){
		return ( open_file != null);
	}
	
	public File getFileArchive(){
		return this.open_file;
	}
	
	public String getFilename(){
		String file = open_file.getAbsolutePath();
		if(file != null)
			return file;
		else
			return "NOFILE";
	}
	
	public String getText(){
		if(this.text!=null)
			return text.getText();
		return null;
	}
	
	
	
	public DoDevEditor(){
		main_window  = new JFrame("DoDevEditor -- Untitled");
		main_window.setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
		main_window.addWindowListener(new Backend.SaveAndClose(this));
        Dimension dim = Toolkit.getDefaultToolkit().getScreenSize();
        main_window.setSize(dim.width,dim.height - 60);
        
        menuBar = new JMenuBar(); 
       
        fileM = new JMenu("File");  
        open_ = new JMenuItem("Open");
        open_.addActionListener(this);
        new_ = new JMenuItem("new File");
        new_.addActionListener(this);
    	save_ = new 	JMenuItem("save");
    	saveas_ = new JMenuItem("Save as...");
    	saveas_.addActionListener(this);
    	save_.addActionListener(this);
    	Exit_ = new JMenuItem("Exit");
    	Exit_.addActionListener(this);
        PrintDlg_= new JMenuItem("Print dialog");
        PrintDlg_.addActionListener(this);
    	fileM.add(open_);
        fileM.add(new_);
        fileM.addSeparator();
        fileM.add(save_);
        fileM.add(saveas_);
        fileM.addSeparator();
        fileM.add(PrintDlg_);
        
        fileM.add(Exit_);

        editM = new JMenu("Edit");  
    	copy_ = new JMenuItem("Copy");
    	paste_= new JMenuItem("Paste");
    	cut_ = new JMenuItem("Cut");
    	delete_ = new JMenuItem("Delete");
    	undo_ = new JMenuItem("Undo");
    	find_ = new JMenuItem("Find");
    	findReplace_ = new JMenuItem("Find and Replace");
    	editM.add(copy_);
    	editM.add(paste_);
    	editM.add(cut_);
    	editM.addSeparator();
    	editM.add(delete_);
    	editM.add(undo_ );
    	editM.addSeparator();
    	editM.add(find_);
    	editM.add(findReplace_);
    	
    
        buildM = new JMenu("Build Command");  
    	build_ = new JMenuItem("- - B u i l d --");
    	configure_build_command_ = new JMenuItem("- configure Build -");
    	view_last_verbose_ =new JMenuItem("W a t c h L o g");
    	run_ =new JMenuItem("R u n");
    	Debug_ = new JMenuItem("Debug");
    	Configure_debug_and_debug_command_ =new JMenuItem("Configure debug");
    	
    	buildM.add(build_);
    	buildM.add(configure_build_command_);
    	buildM.addSeparator();
    	buildM.add(view_last_verbose_);
    	buildM.addSeparator();
    	buildM.add(run_);
    	buildM.addSeparator();
    	buildM.add(Debug_ );
    	buildM.add(Configure_debug_and_debug_command_ );
    	
    	
    	editorM = new JMenu("E d i t o r");
    	configure = new JMenuItem("C o n f i g u r a r");
    	editorM.add(configure);
    	
    	
        
    	About_and_Help = new JMenu("Help");
    	Help_ = new JMenuItem("Help");
    	About_ = new JMenuItem("About");
    	About_.addActionListener(this);
    	About_and_Help.add(Help_);
     	About_and_Help.addSeparator();
    	About_and_Help.add(About_);
    	
        menuBar.add(fileM);
        menuBar.add(editM);
        menuBar.add(buildM);
        menuBar.add(editorM);
        menuBar.add(About_and_Help);
        
        saveas_ .setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_S, ActionEvent.CTRL_MASK));
        new_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_L, ActionEvent.CTRL_MASK));
        cut_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_X, ActionEvent.CTRL_MASK));
        copy_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_C, ActionEvent.CTRL_MASK));
        build_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_V, ActionEvent.CTRL_MASK));
        PrintDlg_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_P, ActionEvent.CTRL_MASK));
        open_.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_A, ActionEvent.CTRL_MASK));
        ///selectI.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_A, ActionEvent.CTRL_MASK));

 
        
        text= new RSyntaxTextArea(20, 60);
        text.setSyntaxEditingStyle(SyntaxConstants.SYNTAX_STYLE_CPLUSPLUS);
        text.setCodeFoldingEnabled(true);
        
        
        
         
        
        
       /// RTextScrollPane sp = new RTextScrollPane(text);
       
        
        //text = new JTextArea(10,10);
        
       /// scrollPane = new JScrollPane( text );
        scrollPane = new RTextScrollPane(text);
        
        text.setSize(dim.width -50, dim.height -100);
        
        
        text.setFont(new Font("Serif", Font.BOLD, 21));
        text.setDragEnabled(true);
        text.setTransferHandler(new MyFileTransferHandler(this));
      
        
        
        Image icon;
		try {
			icon = ImageIO.read(new File("doDevLogo1.png"));
			main_window.setIconImage(icon);
		} catch (IOException e) { 
			e.printStackTrace();
		}
        
        
        
        main_window.setJMenuBar(menuBar);
        ///main_window.add(text);
        main_window.add(scrollPane);
        main_window.setVisible(true);
        
        
        
		
	}

	public void SetText(String text){
		if(this.text != null){
			this.text.setText(text);
		}
	}
	


	@Override
	public void actionPerformed(ActionEvent arg0) {
		JMenuItem test_item = (JMenuItem)arg0.getSource();
		if(test_item == this.About_)
		{
			JOptionPane.showMessageDialog(null, "Powered by : DoDevEditor Team \nRSyntaxTextArea Team  \n All right reserverd a.perez \n you are free to use for now  \n !!! enjoy!!! ");
		}
		else if(test_item == this.open_)
		{
			String code = Backend.OpenAndSaveTextFile.ReadFileByPanel(this.open_);
			text.setText(code);
		}
		else if( this.saveas_ == test_item)
		{
             int SelectOp = JOptionPane.showConfirmDialog(main_window, "Al guardar se sobreescribira el buffer guardad actual ", "Advertencia de Guardado", JOptionPane.YES_NO_OPTION);
			 if(SelectOp == 0)
			 {
			      String text_ = text.getText();
			      this.open_file = Backend.OpenAndSaveTextFile.SaveOption(this.save_, text_);
			 }
			 else
			 {
				 
			 }
		}
		else if( test_item == this.save_ )
		{
			if(this.open_file == null)
			{
			      String text_ = text.getText();
			      this.open_file = Backend.OpenAndSaveTextFile.SaveOption(this.save_, text_);
			}
			else
			{
				 String text_ = text.getText();
				 Backend.OpenAndSaveTextFile.SaveFile(this.open_file, text_);
			}
		}
		else if( test_item == this.Exit_)
		{
			if(text.getText().length() > 0)
			{
				 int SelectOp = JOptionPane.showConfirmDialog(main_window, "Quieres Guardar los Cambios ? ", "Advertencia de Guardado", JOptionPane.YES_NO_OPTION);
				 if(SelectOp == 0)
				 {
						if(this.open_file == null)
						{
						      String text_ = text.getText();
						      this.open_file = Backend.OpenAndSaveTextFile.SaveOption(this.save_, text_);
						}
						else
						{
							 String text_ = text.getText();
							 Backend.OpenAndSaveTextFile.SaveFile(this.open_file, text_);
						}
				 }
				 else
				 {
					 
				 }
			}
			else
			{
				this.main_window.dispose();
				System.exit(1);
			}
			this.main_window.dispose();
			System.exit(1);
		}else if(test_item == this.PrintDlg_)
		{
			String text_ = text.getText();
			Backend.OpenAndSaveTextFile.PrintPage(text_);
		}
		else if( test_item == this.new_)
		{
			if(text.getText().length() > 0)
			{
				 int SelectOp = JOptionPane.showConfirmDialog(main_window, "Quieres Guardar los Cambios ? ", "Advertencia de Guardado", JOptionPane.YES_NO_OPTION);
				 if(SelectOp == 0)
				 {
						if(this.open_file == null)
						{
						      String text_ = text.getText();
						      this.open_file = Backend.OpenAndSaveTextFile.SaveOption(this.save_, text_);
						}
						else
						{
							 String text_ = text.getText();
							 Backend.OpenAndSaveTextFile.SaveFile(this.open_file, text_);
						}
				 }
				 else
				 {
					 text.setText("");
					 
				 }
	     	}
			else
	     	{
				
	     	}
			
	 
				
			 
			
			
 	}
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		

	   if(this.open_file != null){
		   String gettitle = open_file.getName();
		   this.main_window.setTitle("DoDevEditor --" + gettitle);
	   }
	   else{
		   this.main_window.setTitle("DoDevEditor -- UNTITLED");
	   }
	
	
	}
	

	
}
