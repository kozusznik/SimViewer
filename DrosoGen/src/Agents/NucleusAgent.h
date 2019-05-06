#ifndef NUCLEUSAGENT_H
#define NUCLEUSAGENT_H

#include <list>
#include <vector>
#include "../util/report.h"
#include "../util/surfacesamplers.h"
#include "AbstractAgent.h"
#include "../Geometries/Spheres.h"

static ForceName ftype_s2s       = "sphere-sphere";     //internal forces
static ForceName ftype_drive     = "desired movement";
static ForceName ftype_friction  = "friction";

static ForceName ftype_repulsive = "repulsive";         //due to external events with nuclei
static ForceName ftype_body      = "no overlap (body)";
static ForceName ftype_slide     = "no sliding";

static ForceName ftype_hinter    = "sphere-hinter";     //due to external events with shape hinters

static FLOAT fstrength_body_scale     = (FLOAT)0.4;     // [N/um]      TRAgen: N/A
static FLOAT fstrength_overlap_scale  = (FLOAT)0.2;     // [N/um]      TRAgen: k
static FLOAT fstrength_overlap_level  = (FLOAT)0.1;     // [N]         TRAgen: A
static FLOAT fstrength_overlap_depth  = (FLOAT)0.5;     // [um]        TRAgen: delta_o (do)
static FLOAT fstrength_rep_scale      = (FLOAT)0.6;     // [1/um]      TRAgen: B
static FLOAT fstrength_slide_scale    = (FLOAT)1.0;     // unitless
static FLOAT fstrength_hinter_scale   = (FLOAT)0.25;    // [1/um^2]


class NucleusAgent: public AbstractAgent
{
public:
	NucleusAgent(const int _ID, const std::string& _type,
	             const Spheres& shape,
	             const float _currTime, const float _incrTime)
		: AbstractAgent(_ID,_type, geometryAlias, _currTime,_incrTime),
		  geometryAlias(shape),
		  futureGeometry(shape),
		  accels(new Vector3d<FLOAT>[2*shape.noOfSpheres]),
		  //NB: relies on the fact that geometryAlias.noOfSpheres == futureGeometry.noOfSpheres
		  //NB: accels[] and velocities[] together form one buffer (cache friendlier)
		  velocities(accels+shape.noOfSpheres),
		  weights(new FLOAT[shape.noOfSpheres])
	{
		//update AABBs
		geometryAlias.Geometry::updateOwnAABB();
		futureGeometry.Geometry::updateOwnAABB();

		//estimate of number of forces (per simulation round):
		//10(all s2s) + 4(spheres)*2(drive&friction) + 10(neigs)*4(spheres)*4("outer" forces),
		//and "up-rounded"...
		forces.reserve(200);
		velocity_CurrentlyDesired = 0; //no own movement desired yet
		velocity_PersistenceTime  = (FLOAT)2.0;

		for (int i=0; i < shape.noOfSpheres; ++i) weights[i] = (FLOAT)1.0;

		//DEBUG_REPORT("Nucleus with ID=" << ID << " was just created");
	}

	~NucleusAgent(void)
	{
		delete[] accels; //NB: deletes also velocities[], see above
		delete[] weights;

		//DEBUG_REPORT("Nucleus with ID=" << ID << " was just deleted");
	}


protected:
	// ------------- internals state -------------
	/** motion: desired current velocity [um/min] */
	Vector3d<FLOAT> velocity_CurrentlyDesired;

	/** motion: adaptation time, that is, how fast the desired velocity
	    should be reached (from zero movement); this param is in
	    the original literature termed as persistence time and so
	    we keep to that term [min] */
	FLOAT velocity_PersistenceTime;

	// ------------- internals geometry -------------
	/** reference to my exposed geometry ShadowAgents::geometry */
	Spheres geometryAlias;

	/** my internal representation of my geometry, which is exactly
	    of the same form as my ShadowAgent::geometry, even the same noOfSpheres */
	Spheres futureGeometry;

	/** width of the "retention zone" around nuclei that another nuclei
	    shall not enter; this zone simulates cytoplasm around the nucleus;
	    it actually behaves as if nuclei spheres were this much larger
		 in their radii; the value is in microns */
	float cytoplasmWidth = 2.0f;

	// ------------- externals geometry -------------
	/** limiting distance beyond which I consider no interaction possible
	    with other nuclei */
	float ignoreDistance = 10.0f;

	/** locations of possible interaction with nearby nuclei */
	std::list<ProximityPair> proximityPairs_toNuclei;

	/** locations of possible interaction with nearby yolk */
	std::list<ProximityPair> proximityPairs_toYolk;

	/** locations of possible interaction with guiding trajectories */
	std::list<ProximityPair> proximityPairs_tracks;

	// ------------- forces & movement (physics) -------------
	/** all forces that are in present acting on this agent */
	std::vector< ForceVector3d<FLOAT> > forces;

	/** an aux array of acceleration vectors calculated for every sphere, the length
	    of this array must match the length of the spheres in the 'futureGeometry' */
	Vector3d<FLOAT>* const accels;

	/** an array of velocities vectors of the spheres, the length of this array must match
	    the length of the spheres that are exposed (geometryAlias) to the outer world */
	Vector3d<FLOAT>* const velocities;

	/** an aux array of weights of the spheres, the length of this array must match
	    the length of the spheres in the 'futureGeometry' */
	FLOAT* const weights;

	/** essentially creates a new version (next iteration) of 'futureGeometry' given
	    the current content of the 'forces'; note that, in this particular agent type,
	    the 'geometryAlias' is kept synchronized with the 'futureGeometry' so they seem
	    to be interchangeable, but in general setting the 'futureGeometry' might be more
	    rich representation of the current geometry that is regularly "exported" via publishGeometry()
	    and for which the list of ProximityPairs was built during collectExtForces() */
	void adjustGeometryByForces(void)
	{
		//TRAgen paper, eq (1):
		//reset the array with final forces (which will become accelerations soon)
		for (int i=0; i < futureGeometry.noOfSpheres; ++i) accels[i] = 0;

		//collect all forces acting on every sphere to have one overall force per sphere
		for (const auto& f : forces) accels[f.hint] += f;

#ifdef DEBUG
		if (detailedReportingMode)
		{
			for (const auto& f : forces) REPORT(ID << ": ||=" << f.len() << "\tforce " << f);

			std::ostringstream forcesReport;
			forcesReport << ID << ": final forces";
			for (int i=0; i < futureGeometry.noOfSpheres; ++i)
				forcesReport << ", |" << i << "|=" << accels[i].len();
			REPORT(forcesReport.str());
		}
#endif
		//now, translation is a result of forces:
		for (int i=0; i < futureGeometry.noOfSpheres; ++i)
		{
			//calculate accelerations: F=ma -> a=F/m
			//TODO: volume of a sphere should be taken into account
			accels[i] /= weights[i];

			//velocities: v=at
			velocities[i] += (FLOAT)incrTime * accels[i];

			//displacement: |trajectory|=vt
			futureGeometry.centres[i] += (FLOAT)incrTime * velocities[i];
		}

		//update AABB to the new geometry
		futureGeometry.Geometry::updateOwnAABB();

		//all forces processed...
		forces.clear();
	}

	// ------------- to implement one round of simulation -------------
	void advanceAndBuildIntForces(const float futureGlobalTime) override
	{
		//call the "texture hook"!
		advanceAgent(futureGlobalTime);

		//add forces on the list that represent how and where the nucleus would like to move
		//TRAgen paper, eq (2): Fdesired = weight * drivingForceMagnitude
		//NB: the forces will act rigidly on the full nucleus
		for (int i=0; i < futureGeometry.noOfSpheres; ++i)
		{
			forces.emplace_back(
				(weights[i]/velocity_PersistenceTime) * velocity_CurrentlyDesired,
				futureGeometry.centres[i],i, ftype_drive );
		}

#ifdef DEBUG
		//export forces for display:
		forcesForDisplay = forces;
#endif
		//increase the local time of the agent
		currTime += incrTime;
	}

	void adjustGeometryByIntForces(void) override
	{
		adjustGeometryByForces();
	}

	void collectExtForces(void) override
	{
		//damping force (aka friction due to the environment,
		//an ext. force that is independent of other agents)
		//TRAgen paper, eq. (3)
		for (int i=0; i < futureGeometry.noOfSpheres; ++i)
		{
			forces.emplace_back(
				(-weights[i]/velocity_PersistenceTime) * velocities[i],
				futureGeometry.centres[i],i, ftype_friction );
		}

		//scheduler, please give me ShadowAgents that are not further than ignoreDistance
		//(and the distance is evaluated based on distances of AABBs)
		std::list<const ShadowAgent*> nearbyAgents;
		Officer->getNearbyAgents(this,ignoreDistance, nearbyAgents);

#ifdef DEBUG
		if (detailedReportingMode)
			REPORT("ID " << ID << ": Found " << nearbyAgents.size() << " nearby agents");
#endif
		//those on the list are ShadowAgents who are potentially close enough
		//to interact with me and these I need to inspect closely
		proximityPairs_toNuclei.clear();
		proximityPairs_toYolk.clear();
		proximityPairs_tracks.clear();
		for (const auto sa : nearbyAgents)
		{
			if ( (sa->getAgentType())[0] == 'n' )
				geometry.getDistance(sa->getGeometry(),proximityPairs_toNuclei, (void*)((const NucleusAgent*)sa));
			else
			{
				if ( (sa->getAgentType())[0] == 'y' )
					geometry.getDistance(sa->getGeometry(),proximityPairs_toYolk);
				else
					geometry.getDistance(sa->getGeometry(),proximityPairs_tracks);
			}
		}

#ifdef DEBUG
		if (detailedReportingMode)
		{
			REPORT("ID " << ID << ": Found " << proximityPairs_toNuclei.size() << " proximity pairs to nuclei");
			REPORT("ID " << ID << ": Found " << proximityPairs_toYolk.size()   << " proximity pairs to yolk");
			REPORT("ID " << ID << ": Found " << proximityPairs_tracks.size()   << " proximity pairs with guiding trajectories");
		}
#endif
		//now, postprocess the proximityPairs, that is, to
		//convert proximityPairs_toNuclei to forces according to TRAgen rules
		Vector3d<FLOAT> f,g; //tmp vectors
		for (const auto& pp : proximityPairs_toNuclei)
		{
			if (pp.distance > 0)
			{
#ifdef DEBUG
				if (detailedReportingMode)
					REPORT(ID << ": repulsive  pp.distance=" << pp.distance);
#endif
				//no collision
				if (pp.distance < 3.0) //TODO: replace 3.0 with some function of fstrength_rep_scale
				{
					//distance not too far, repulsion makes sense here
					//
					//unit force vector (in the direction "away from the other buddy")
					f  = pp.localPos;
					f -= pp.otherPos;
					f.changeToUnitOrZero();

					//TRAgen paper, eq. (4)
					forces.emplace_back(
						(fstrength_overlap_level * std::exp(-pp.distance / fstrength_rep_scale)) * f,
						futureGeometry.centres[pp.localHint],pp.localHint, ftype_repulsive );
				}
			}
			else
			{
				//collision, pp.distance <= 0
				//NB: in collision, the other surface is within local volume, so
				//    the vector local->other actually points in the opposite direction!
				//    (as local surface further away than other surface from local centre)

				//body force
				//
				//unit force vector (in the direction "away from the other buddy")
				f  = pp.otherPos;
				f -= pp.localPos;
				f.changeToUnitOrZero();

				FLOAT fScale = fstrength_overlap_level;
				if (-pp.distance > fstrength_overlap_depth)
				{
					//in the non-calm response zone (where force increases with the penetration depth)
					fScale += fstrength_overlap_scale * (-pp.distance - fstrength_overlap_depth);
				}

				//TRAgen paper, eq. (5)
				forces.emplace_back( fScale * f,
					futureGeometry.centres[pp.localHint],pp.localHint, ftype_body );

#ifdef DEBUG
				if (detailedReportingMode)
					REPORT(ID << ": body  pp.distance=" << pp.distance << " |force|=" << fScale*f.len());
#endif
				//sliding force
				//
				//difference of velocities
				g  = ((const NucleusAgent*)pp.callerHint)->getVelocityOfSphere(pp.otherHint);
				g -= velocities[pp.localHint];

#ifdef DEBUG
				if (detailedReportingMode)
					REPORT(ID << ": slide oID=" << ((const NucleusAgent*)pp.callerHint)->ID << " |velocityDiff|=" << g.len());
#endif
				//subtract from it the component that is parallel to this proximity pair
				f *= dotProduct(f,g); //f is now the projection of g onto f
				g -= f;               //g is now the difference of velocities without the component
				                      //that is parallel with the proximity pair

				//TRAgen paper, somewhat eq. (6)
				g *= fstrength_slide_scale * weights[pp.localHint]/velocity_PersistenceTime;
				// "surface friction coeff" | velocity->force, the same as for ftype_drive
				forces.emplace_back( g,
					futureGeometry.centres[pp.localHint],pp.localHint, ftype_slide );
#ifdef DEBUG
				Officer->reportOverlap(-pp.distance);
#endif
			}
		}

		//non-TRAgen new force, driven by the offset distance from the position expected by the shape hinter,
		//it converts proximityPairs_toYolk to forces
		for (const auto& pp : proximityPairs_toYolk)
		if (pp.localHint == 0) //consider pair related only to the first sphere of a nucleus
		{
			//unit force vector (in the direction "towards the shape hinter")
			f  = pp.otherPos;
			f -= pp.localPos;
			f.changeToUnitOrZero();

#ifdef DEBUG
			if (detailedReportingMode)
				REPORT(ID << ": hinter pp.distance=" << pp.distance);
#endif
			//the get-back-to-hinter force
			f *= 2*fstrength_overlap_level * std::min(pp.distance*pp.distance * fstrength_hinter_scale,(FLOAT)1);

			//apply the same force to all spheres
			for (int i=0; i < futureGeometry.noOfSpheres; ++i)
				forces.emplace_back( f, futureGeometry.centres[i],i, ftype_hinter );
		}

#ifdef DEBUG
		//append forces to forcesForDisplay, make a copy (push_back, not emplace_back)!
		for (const auto& f : forces)
			forcesForDisplay.push_back(f);
#endif
	}

	void adjustGeometryByExtForces(void) override
	{
		adjustGeometryByForces();
	}

	void publishGeometry(void) override
	{
		//promote my NucleusAgent::futureGeometry to my ShadowAgent::geometry, which happens
		//to be overlaid/mapped-over with NucleusAgent::geometryAlias (see the constructor)
		for (int i=0; i < geometryAlias.noOfSpheres; ++i)
		{
			geometryAlias.centres[i] = futureGeometry.centres[i];
			geometryAlias.radii[i]   = futureGeometry.radii[i] +cytoplasmWidth;
		}

		//update AABB
		geometryAlias.Geometry::updateOwnAABB();
	}

public:
	const Vector3d<FLOAT>& getVelocityOfSphere(const long index) const
	{
#ifdef DEBUG
		if (index >= geometryAlias.noOfSpheres)
			throw ERROR_REPORT("requested sphere index out of bound.");
#endif

		return velocities[index];
	}

protected:
	// ------------- rendering -------------
	void drawMask(DisplayUnit& du) override
	{
		const int color = 2;

		//if not selected: draw cells with no debug bit
		//if     selected: draw cells as a global debug object
		int dID = ID << 17;
		int gdID = ID*40 +5000;
		//NB: 'd'ID = is for 'd'rawing, not for 'd'ebug !

		//draw spheres
		for (int i=0; i < futureGeometry.noOfSpheres; ++i)
		{
			du.DrawPoint( detailedDrawingMode?gdID:dID ,futureGeometry.centres[i],futureGeometry.radii[i],color);
			++dID; ++gdID; //just update both counters
		}

		//velocities -- global debug
		//for (int i=0; i < futureGeometry.noOfSpheres; ++i)
		{
			int i=0;
			du.DrawVector(gdID++, futureGeometry.centres[i],velocities[i], 0); //white color
		}

		//red lines with overlapping proximity pairs to nuclei
		//(if detailedDrawingMode is true, these lines will be drawn later as "local debug")
		if (!detailedDrawingMode)
		{
			for (const auto& p : proximityPairs_toNuclei)
			if (p.distance < 0)
				du.DrawLine(gdID++, p.localPos,p.otherPos, 1);
		}
	}

#ifdef DEBUG
	std::vector< ForceVector3d<FLOAT> > forcesForDisplay;
#endif

	void drawForDebug(DisplayUnit& du) override
	{
		//render only if under inspection
		if (detailedDrawingMode)
		{
			const int color = 2;
			int dID = ID << 17 | 1 << 16; //enable debug bit

			//cell centres connection "line" (green):
			for (int i=1; i < futureGeometry.noOfSpheres; ++i)
				du.DrawLine(dID++, futureGeometry.centres[i-1],futureGeometry.centres[i], color);

			//draw agent's periphery (as blue spheres)
			//NB: showing the cell outline, that is now updated from the futureGeometry,
			//and stored already in the geometryAlias
			SphereSampler<float> ss;
			Vector3d<float> periPoint;
			int periPointCnt=0;

			for (int S = 0; S < geometryAlias.noOfSpheres; ++S)
			{
				ss.resetByStepSize(geometryAlias.radii[S], 2.6f);
				while (ss.next(periPoint))
				{
					periPoint += geometryAlias.centres[S];

					//draw the periPoint only if it collides with no (and excluding this) sphere
					if (geometryAlias.collideWithPoint(periPoint, S) == -1)
					{
						++periPointCnt;
						du.DrawPoint(dID++, periPoint, 0.3f, 3);
					}
				}
			}
			DEBUG_REPORT(IDSIGN << "surface consists of " << periPointCnt << " spheres");

			//red lines with overlapping proximity pairs to nuclei
			for (const auto& p : proximityPairs_toNuclei)
			if (p.distance < 0)
				du.DrawLine(dID++, p.localPos,p.otherPos, 1);

			//neighbors:
			//white line for the most inner spheres, yellow for second most inner
			//both showing proximity pairs to yolk (shape hinter)
			for (const auto& p : proximityPairs_toYolk)
			if (p.localHint < 2)
				du.DrawLine(dID++, p.localPos, p.otherPos, (int)(p.localHint*6));

			//magenta lines with trajectory guiding vectors
			for (const auto& p : proximityPairs_tracks)
			if (p.distance > 0)
				du.DrawVector(dID++, p.localPos,p.otherPos-p.localPos, 5);

#ifdef DEBUG
			//forces:
			for (const auto& f : forcesForDisplay)
			{
				int color = 2; //default color: green (for shape hinter)
				//if      (f.type == ftype_s2s)      color = 4; //cyan
				//else if (f.type == ftype_drive)    color = 5; //magenta
				//else if (f.type == ftype_friction) color = 6; //yellow
				if      (f.type == ftype_body)      color = 4; //cyan
				else if (f.type == ftype_repulsive || f.type == ftype_drive) color = 5; //magenta
				else if (f.type == ftype_slide)     color = 6; //yellow
				else if (f.type == ftype_friction)  color = 3; //blue
				else if (f.type != ftype_hinter)    color = -1; //don't draw
				if (color > 0) du.DrawVector(dID++, f.base,f, color);
			}
#endif
			//velocities:
			std::ostringstream velocitiesReport;
			//
			//choose 2nd sphere if avail, else 1st sphere if avail, else none (then: Idx == -1)
			const int velocityReportIdx = std::min(2,futureGeometry.noOfSpheres) -1;
			if (velocityReportIdx == -1)
				velocitiesReport << ID << ": no spheres -> no velocities";
			else
				velocitiesReport << ID << ": velocity[" << velocityReportIdx << "]=" << velocities[velocityReportIdx];
			//
			for (int i=0; i < futureGeometry.noOfSpheres; ++i)
				velocitiesReport << ", |" << i << "|=" << velocities[i].len();
			REPORT(velocitiesReport.str());
		}
	}

	void drawMask(i3d::Image3d<i3d::GRAY16>& img) override
	{
		//shortcuts to the mask image parameters
		const i3d::Vector3d<float>& res = img.GetResolution().GetRes();
		const Vector3d<FLOAT>       off(img.GetOffset().x,img.GetOffset().y,img.GetOffset().z);

		//shortcuts to our Own spheres
		const Vector3d<FLOAT>* const centresO = futureGeometry.getCentres();
		const FLOAT* const radiiO             = futureGeometry.getRadii();
		const int iO                          = futureGeometry.getNoOfSpheres();

		//project and "clip" this AABB into the img frame
		//so that voxels to sweep can be narrowed down...
		//
		//   sweeping position and boundaries (relevant to the 'img')
		Vector3d<size_t> curPos, minSweepPX,maxSweepPX;
		futureGeometry.AABB.exportInPixelCoords(img, minSweepPX,maxSweepPX);
		//
		//micron coordinate of the running voxel 'curPos'
		Vector3d<FLOAT> centre;

		//sweep and check intersection with spheres' volumes
		for (curPos.z = minSweepPX.z; curPos.z < maxSweepPX.z; curPos.z++)
		for (curPos.y = minSweepPX.y; curPos.y < maxSweepPX.y; curPos.y++)
		for (curPos.x = minSweepPX.x; curPos.x < maxSweepPX.x; curPos.x++)
		{
			//get micron coordinate of the current voxel's centre
			centre.x = ((FLOAT)curPos.x +0.5f) / res.x;
			centre.y = ((FLOAT)curPos.y +0.5f) / res.y;
			centre.z = ((FLOAT)curPos.z +0.5f) / res.z;
			centre += off;

			//check the current voxel against all spheres
			for (int i = 0; i < iO; ++i)
			{
				//if sphere's surface would be 2*lenPXhd thick, would the voxel's center be in?
				if ((centre-centresO[i]).len() <= radiiO[i])
				{
#ifdef DEBUG
					i3d::GRAY16 val = img.GetVoxel(curPos.x,curPos.y,curPos.z);
					if (val > 0 && val != (i3d::GRAY16)ID)
						REPORT(ID << " overwrites mask at " << curPos);
#endif
					img.SetVoxel(curPos.x,curPos.y,curPos.z, (i3d::GRAY16)ID);
				}
			}
		}
	}

	void drawForDebug(i3d::Image3d<i3d::GRAY16>& img) override
	{
		drawMask(img);
	}
};
#endif
