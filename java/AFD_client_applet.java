
import java.applet.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;

public class
AFD_client_applet extends Applet implements ActionListener
{
   public static final int AFD_PORT = 4444;
   Socket                  s;
   BufferedReader          in;
   PrintWriter             outs;
   TextField               inputfield;
   TextArea                outputarea;
   StreamListener          listener;

   public String
   getAppletInfo()
   {
        return (new String("AFD TCP client by Holger Kiehl"));
   }

   public void
   init()
   {
      try
      {
         // Initialize socket stuff
         s   = new Socket(this.getCodeBase().getHost(), AFD_PORT);
//         s   = new Socket("localhost", AFD_PORT);
	 in  = new BufferedReader(new InputStreamReader(s.getInputStream()));
	 outs = new PrintWriter(s.getOutputStream());

	 // Initialize AWT stuff
	 inputfield = new TextField();
	 outputarea = new TextArea(10, 80);
	 outputarea.setEditable(false);
	 this.setLayout(new BorderLayout());
	 this.add("North", inputfield);
	 this.add("Center", outputarea);
	 inputfield.addActionListener(this);

// System.out.println("Anfang!");
	 listener = new StreamListener(in, outputarea);

	 this.showStatus("Connected to " + s.getInetAddress().getHostName() +
	                 ":" + s.getPort());
      }

      catch(IOException e)
      {
         this.showStatus(e.toString());
      }
   }

   // Handle user input
   public void
   actionPerformed(ActionEvent e)
   {
      Object source = e.getSource();

      if (source == inputfield)
      {
         String str = inputfield.getText();

         str += '\r';
// System.out.println("Input: " + str);
         outs.println(str);
	 outs.flush();

         // Clear input field
         inputfield.setText("");
      }

      return;
   }
}


class StreamListener extends Thread
{
   BufferedReader in;
   TextArea output;

   public
   StreamListener(BufferedReader in, TextArea output)
   {
      this.in = in;

      this.output = output;
      this.start();
   }

   public void
   run()
   {
      String line;

      try
      {
         for (;;)
	 {
	    line = in.readLine();
// System.out.println("Daemon: " + line);
	    if (line == null)
	    {
// System.out.println("Ende!");
               break;
	    }
	    output.append(line + '\n');
	 }
      }

      catch(IOException e)
      {
         output.setText(e.toString());
      }

      finally
      {
         output.setText("Connection closed by server.");
      }
   }
}
