/**
BSD 2-Clause License

Copyright (c) 2019, Vladimír Ulman
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


package de.mpicbg.ulman.simviewer.elements;

import cleargl.GLVector;
import graphics.scenery.Node;

/** Corresponds to one element that simulator's DrawLine() can send;
    graphically, line is essentially a vector without an arrow head.
    The class governs all necessary pieces of information to display
    the line, and the Scenery's Nodes are pointed inside this class
    to (re)fetch the actual display data/instructions. */
public class Line extends Vector
{
	public Line()             { super(); }    //without connection to Scenery
	public Line(final Node l) { super(l); }   //  with  connection to Scenery

	/** converts a line, given via its end positions, into a vector-like representation */
	public void reset(final GLVector posA, final GLVector posB, final int color)
	{
		//essentially supplies the functionality of the Vector::update(),
		//difference is in the semantics of the input
		base.set(0, posA.x());
		base.set(1, posA.y());
		base.set(2, posA.z());
		vector.set(0, posB.x()-posA.x());
		vector.set(1, posB.y()-posA.y());
		vector.set(2, posB.z()-posA.z());
		this.color = color;

		//also update the vector's auxScale:
		applyScale(1f);
	}

	public void update(final Line l)
	{
		super.update(l);
	}
}
