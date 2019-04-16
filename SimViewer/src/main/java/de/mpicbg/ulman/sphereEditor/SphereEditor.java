package de.mpicbg.ulman.sphereEditor;

import graphics.scenery.controls.InputHandler;
import org.scijava.ui.behaviour.ClickBehaviour;
import de.mpicbg.ulman.simviewer.DisplayScene;
import de.mpicbg.ulman.simviewer.aux.Point;

public class SphereEditor
{
	//reference on the DisplayScene
	DisplayScene scene = null;

	//aux container to define the current geometry
	final myPoint[] geometry = new myPoint[4];
	final int ID = 1 << 17;

	//modifiers...
	int selectedSphere = 0;
	float stepSize = 0.5f;

	//the DisplayScene.myPoint with some extended functionalities...
	class myPoint extends Point
	{
		void updateCentre(final int axis, final float dx)
		{
			centre.set(axis, centre.get(axis)+dx);
		}

		void updateRadius(final float dr)
		{
			radius.set(0, radius.x()+dr);
			radius.set(1, radius.x());
			radius.set(2, radius.x());
		}

		void setColour(final int c)
		{
			color = c;
		}
	}


	public static void main(String... args)
	{
		new SphereEditor().mmmain();
	}

	public
	void mmmain()
	{
		scene = new DisplayScene(new float[] {  0.f,  0.f,  0.f},
		                         new float[] {480.f,220.f,220.f});

		final float xCentre  = 300.0f;
		final float yCentre  = 150.0f;
		final float zCentre  = 150.0f;

		final Thread GUIwindow  = new Thread(scene);
		try {
			//start the rendering window first
			GUIwindow.start();

			//give the GUI window some time to settle down, and populate it
			scene.waitUntilSceneIsReady();
			System.out.println("SimViewer is ready!");

			//display stuff
			scene.ToggleDisplayAxes();

			//now init the nucleus shape
			geometry[0] = new myPoint();
			geometry[0].centre.set(0, xCentre -30.0f);
			geometry[0].centre.set(1, yCentre);
			geometry[0].centre.set(2, zCentre);
			geometry[0].updateRadius(10.0f);
			geometry[0].color = 1;
			scene.addUpdateOrRemovePoint(ID+0, geometry[0]);

			geometry[1] = new myPoint();
			geometry[1].centre.set(0, xCentre -8.0f);
			geometry[1].centre.set(1, yCentre);
			geometry[1].centre.set(2, zCentre);
			geometry[1].updateRadius(20.0f);
			geometry[1].color = 2;
			scene.addUpdateOrRemovePoint(ID+1, geometry[1]);

			geometry[2] = new myPoint();
			geometry[2].centre.set(0, xCentre +8.0f);
			geometry[2].centre.set(1, yCentre);
			geometry[2].centre.set(2, zCentre);
			geometry[2].updateRadius(20.0f);
			geometry[2].color = 2;
			scene.addUpdateOrRemovePoint(ID+2, geometry[2]);

			geometry[3] = new myPoint();
			geometry[3].centre.set(0, xCentre +30.0f);
			geometry[3].centre.set(1, yCentre);
			geometry[3].centre.set(2, zCentre);
			geometry[3].updateRadius(10.0f);
			geometry[3].color = 2;
			scene.addUpdateOrRemovePoint(ID+3, geometry[3]);

			//add controlls...
			// '1'-'4'    - chooses which sphere's centre to manipulate
			// j,l        - moves in x axis
			// k,i        - moves in y axis
			// o,9        - moves in z axis
			// 7,u        - inc/dec radius
			// shift 7,u  - adjusts the step size (for both radius and position displacements)

			while (scene.exposeInputHandler() == null) Thread.sleep(1000);
			final InputHandler ih = scene.exposeInputHandler();

			ih.addBehaviour( "VU_ksi", new KeyStopIt());
			ih.addKeyBinding("VU_ksi", "E");

			ih.addBehaviour( "VU_ksfs", new KeySelectSphere(0));
			ih.addKeyBinding("VU_ksfs", "1");
			ih.addBehaviour( "VU_ksss", new KeySelectSphere(1));
			ih.addKeyBinding("VU_ksss", "2");
			ih.addBehaviour( "VU_ksts", new KeySelectSphere(2));
			ih.addKeyBinding("VU_ksts", "3");
			ih.addBehaviour( "VU_ksos", new KeySelectSphere(3));
			ih.addKeyBinding("VU_ksos", "4");

			ih.addBehaviour( "VU_kasx", new KeyAdjustSphere('x'));
			ih.addKeyBinding("VU_kasx", "J");
			ih.addBehaviour( "VU_kasX", new KeyAdjustSphere('X'));
			ih.addKeyBinding("VU_kasX", "L");
			ih.addBehaviour( "VU_kasy", new KeyAdjustSphere('y'));
			ih.addKeyBinding("VU_kasy", "K");
			ih.addBehaviour( "VU_kasY", new KeyAdjustSphere('Y'));
			ih.addKeyBinding("VU_kasY", "I");
			ih.addBehaviour( "VU_kasz", new KeyAdjustSphere('z'));
			ih.addKeyBinding("VU_kasz", "9");
			ih.addBehaviour( "VU_kasZ", new KeyAdjustSphere('Z'));
			ih.addKeyBinding("VU_kasZ", "O");
			ih.addBehaviour( "VU_kasr", new KeyAdjustSphere('r'));
			ih.addKeyBinding("VU_kasr", "U");
			ih.addBehaviour( "VU_kasR", new KeyAdjustSphere('R'));
			ih.addKeyBinding("VU_kasR", "7");

			ih.addBehaviour( "VU_kass", new KeyAdjustSphere('-'));
			ih.addKeyBinding("VU_kass", "shift U");
			ih.addBehaviour( "VU_kasS", new KeyAdjustSphere('+'));
			ih.addKeyBinding("VU_kasS", "shift 7");

			//how this can be stopped?
			//control shall never stop unless 'stop key' is hit in which case it signals GUI to stop
			//GUI can stop anytime (either from 'stop key' or just by user closing the window)

			//wait for the GUI window to finish
			GUIwindow.join();

			//signal the remaining threads to stop
			//if (GUIcontrol.isAlive()) GUIcontrol.interrupt();
		} catch (InterruptedException e) {
			System.out.println("We've been interrupted while waiting for our threads to close...");
			e.printStackTrace();
		}
	}

	//callback classes:
	private class KeyStopIt implements ClickBehaviour
	{
		@Override
		public void click( final int x, final int y )
		{
			scene.stop();
		}
	}

	private class KeySelectSphere implements ClickBehaviour
	{
		final int idx;
		KeySelectSphere(final int idx) { this.idx = idx; }

		@Override
		public void click( final int x, final int y )
		{
			geometry[selectedSphere].color = 2;
			scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);

			selectedSphere = idx;

			geometry[selectedSphere].color = 1;
			scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
		}
	}

	private class KeyAdjustSphere implements ClickBehaviour
	{
		final char key;
		KeyAdjustSphere(final char key) { this.key = key; }

		@Override
		public void click( final int x, final int y )
		{
			switch (key)
			{
			case 'x':
				geometry[selectedSphere].updateCentre(0,-stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'X':
				geometry[selectedSphere].updateCentre(0,+stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'y':
				geometry[selectedSphere].updateCentre(1,-stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'Y':
				geometry[selectedSphere].updateCentre(1,+stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'z':
				geometry[selectedSphere].updateCentre(2,-stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'Z':
				geometry[selectedSphere].updateCentre(2,+stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;

			case 'r':
				geometry[selectedSphere].updateRadius(-stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;
			case 'R':
				geometry[selectedSphere].updateRadius(+stepSize);
				scene.addUpdateOrRemovePoint(ID+selectedSphere, geometry[selectedSphere]);
				break;

			case '-':
				stepSize = Math.max(stepSize-0.1f, 0.1f);
				System.out.println("new step size: "+stepSize);
				break;
			case '+':
				stepSize += 0.1f;
				System.out.println("new step size: "+stepSize);
				break;
			}
		}
	}
}
