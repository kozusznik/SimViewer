package de.mpicbg.ulman.sphereEditor;

import java.io.InputStreamReader;
import java.io.IOException;

import de.mpicbg.ulman.simviewer.DisplayScene;
import de.mpicbg.ulman.simviewer.aux.Point;

/**
 * Adapted from TexturedCubeJavaExample.java from the scenery project,
 * originally created by kharrington on 7/6/16.
 *
 * This file was created and is being developed by Vladimir Ulman, 2018.
 */
public class CommandScene implements Runnable
{
	/** constructor to create connection to a displayed window */
	public CommandScene(final DisplayScene _scene)
	{
		scene = _scene;
	}

	/** reference on the controlled rendering display */
	private final DisplayScene scene;


	/** reads the console and dispatches the commands */
	public void run()
	{
		System.out.println("Key listener: Started.");
		final InputStreamReader console = new InputStreamReader(System.in);
		try {
			while (true)
			{
				if (console.ready())
					processKey(console.read());
				else
					Thread.sleep(1000);
			}
		}
		catch (IOException e) {
			System.out.println("Key listener: Error reading the console.");
			e.printStackTrace();
		}
		catch (InterruptedException e) {
			System.out.println("Key listener: Stopped.");
		}
	}


	private
	void processKey(final int key) throws InterruptedException
	{
		switch (key)
		{
		case 'h':
			System.out.println("List of accepted commands:");
			System.out.println("h - Shows this help message");
			System.out.println("q - Quits the program");
			System.out.println("o - Overviews the current settings");
			break;

		case 'q':
			scene.stop();
			//don't wait for GUI to tell me to stop
			throw new InterruptedException("Stop myself now.");
		default:
			if (key != '\n') //do not respond to Enter keystrokes
				System.out.println("Not recognized command, no action taken");
		}
	}
}
