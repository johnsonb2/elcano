/*  
Elcano Module C4: Path Planner.

Copy the following software files to an Elcano_C6_Navigator directory:
  Elcano_C4_Planner.ino; add new tabs with the following names and copy contents
  Common.cpp
  Common.h
  IO.h

Output to C6: Desired route and speed curves and whether it exits a road or changes lane.
Input: RNDF, MDF and initial position files from memory stick.
Input from C5: Speed and position of obstacles.
Input from C6: Position, orientation, velocity and acceleration.


Files:
RNDF (Route Network Definition File). Format is defined on 
http://archive.darpa.mil/grandchallenge/docs/RNDF_MDF_Formats_031407.pdf. 
This is a digital map of all roads in the area where the vehicle will be operating. 
The camera will locate road edges or lane markers and the vehicle will follow them. 
Thus location can be determined primarily from odometry.

MDF (Mission Definition File). These are latitudes and longitudes that the vehicle 
is required to visit.

Initial position. Specifies the starting location and orientation. Velocity is zero. 
If this is a file, it is read by C4 (Path Planner) and passed to C6 (Navigator). 
If it is user input, it is read by C6 (Navigator).

The present C4 module is the only micro-controller that has a file system.

USB: All Arduinos contain a USB port that lets them download code from a PC.
Since a USB connection is not a network, the Arduinos cannot talk to each other over USB.
To enable USB, one of the Arduinos must include a USB server. If there is a USB server, it might be
C4, which may have an OS, or
C6, which needs to talk to lots of instruments, or
C7, which may have a USB link to a camera, or
we could have another processor whose sole function is communication.
*/

/*---------------------------------------------------------------------------------------*/ 
#include "Common.h"
#include "IO.h"
#define PI ((float) 3.1415925)
#ifndef NULL
#define NULL 0 
#endif
void DataReady();
extern bool DataAvailable;

#define MAX_WAYPOINTS 10
/*   There are two coordinate systems.
     MDF and RNDF use latitude and longitude.
     C3 and C6 assume that the earth is flat at the scale that they deal with
*/

/*  Nodes and Links define the road network.
    The path for the robot is an array of pointers to Nodes.
    Nodes and Links come from the RNDF (Route Network Definition File).
    MDF (Mission Definition File) is given.
    Origin is the initial position of the robot and (0,0) on the mm coordinate system. 
    Start is the position of the robot as it begins a new leg of the journey.  
    Destination is next position in MDF.
    Path joins Start and Destination. Finding it is the major task 
    of this Path Planner module.
    Route is a finer scale list of waypoints from present or recent position.
    Exit is the route from Path[last]-> location to Destination.
*/
struct curve Links[20];
struct junction Nodes[16] = {
  -140828,  221434, &Nodes[3],  &Nodes[1],  NULL,       NULL,      0, 0, 0, 0,  // 0
  -140986,   88800, &Nodes[0],  &Nodes[2],  &Nodes[5],  NULL,      0, 0, 0, 0,  // 1
  -144313,  -42065, &Nodes[1],  &Nodes[6],  NULL,       NULL,      0, 0, 0, 0,  // 2
  -78568,   222090, &Nodes[0],  &Nodes[4],  NULL,       NULL,      0, 0, 0, 0,  // 3
  -38276,   222290, &Nodes[3],  &Nodes[5],  NULL,       NULL,      0, 0, 0, 0,  // 4
  -39558,    87844, &Nodes[1],  &Nodes[4],  &Nodes[8],  &Nodes[6], 0, 0, 0, 0,  // 5
  -46528,   -41631, &Nodes[2],  &Nodes[5],  &Nodes[7],  &Nodes[9], 0, 0, 0, 0,  // 6
  -45764,  -135413, &Nodes[6],  &Nodes[10], NULL,       NULL,      0, 0, 0, 0,  // 7
  51834,     87232, &Nodes[5],  &Nodes[9],  &Nodes[14], NULL,      0, 0, 0, 0,  // 8
  53041,    -41220, &Nodes[6],  &Nodes[8],  &Nodes[10], &Nodes[13],0, 0, 0, 0,  // 9
  53438,   -133901, &Nodes[7],  &Nodes[9],  &Nodes[11], NULL,      0, 0, 0, 0,  // 10
  108750,  -134590, &Nodes[10], &Nodes[12], NULL,       NULL,      0, 0, 0, 0,  // 11
  130021,  -143108, &Nodes[11], NULL,       NULL,       NULL,      0, 0, 0, 0,  // 12
  182559,   -41031, &Nodes[9],  NULL,       NULL,       NULL,      0, 0, 0, 0,  // 13
  177598,    86098, &Nodes[8],  &Nodes[15], NULL,       NULL,      0, 0, 0, 0,  // 14
  170313,    69008, &Nodes[14], NULL,       NULL,       NULL,      0, 0, 0, 0   // 15
};
class waypoint Origin, Start;
struct curve Route, Exit;
struct junction *Path[20];
class waypoint Destination;

// EDIT for route
// CONES includes start and stop
#define CONES 5
long goal_lat[CONES] = {  47621881,   47621825,   47623144,   47620616,   47621881};
long goal_lon[CONES] = {-122349894, -122352120, -122351987, -122351087, -122349894};

  
waypoint mission[MAX_WAYPOINTS];  // aka MDF
int waypoints;

/*---------------------------------------------------------------------------------------*/ 
void initialize()
{
  Origin.Evector_x1000 = INVALID; 
  Origin.Nvector_x1000 = INVALID; 
  Origin.east_mm = 0;
  Origin.north_mm = 0;  
  Origin.latitude = INVALID;
  Origin.longitude = INVALID;  
  
  /* Choose the MDF that defines the current mission 
     The MDF consists of a sequence of points given in latitude and longitude. 
     The mission is to visit each of these points in order. 
     MDF = ReadMDF(mission); */

     
  /* Select all RNDF files that are appropriate to this mission. 
     Initially we will assume that there is a single RNDF file.
     ReadRNDF(mission); */
  /* The RNDF defines lane segments of a path.
     Use them to fill in Links and Nodes.
     At many of the RNDF segments, ther is no choice to make; it just leads to the next waypoint.
     Find those segments that represent intersections or other choice points.
     Then construct a graph whose nodes are intersections and whose arcs are labeled with the
     expected time and distance to move between nodes.
     ConstructNetwork() */
     
  /* If (initial position is provided)  // used when GPS is unavailable or inaccurate
     {
        ReadPosition();  // latitude and longitude
        // The origin in the (east_mm, north_mm) coordinate system is defined to be the
        // latitude and longitude at which the robot starts. 
        // (Evector, Nvector) is initial robot attitude.
        SendInitialPosition(C6);  // latitude and longitude
     }
     */
     
  /* Convert latitude and longitude positions to flat earth coordinates.
     for each waypoint in Origin, Start, Destination, MDF, Nodes, Links
       LatatitudeLongitude(waypoint *Waypoint);
  */
 
     /* Read vehicle position, attitude and velocity.
     ReceiveState(C6); 
     fill in Origin.latitude, Origin.longitude, Origin.bearing 
     set Start to Origin; */
     
     
     /* Plan a Path (using RNDF) between each Node. 
     Use the A* algorithm as given in
     Robin Murphy, Introduction to AI Robotics, MIT Press.
     Steve Rabin (ed) AI Game Programming Wisdom, Charles River Media. */
     
     
/*   for (i = 0; i < waypoints; i++)
     {
       PlanPath (waypoint[i]);
     }
     PlanPath must also receive obstacle information from C5.
     The output from PlanPath is a sequence of waypoints Path[] where each waypoint
     is associated with a junction in the RNDF.  We must then supplement the junction
     points with all the intermediate non-junction points in the RNDF so that
     the vehicle can follow curves in the road. The first section of this is Route.
     
     We now have a sequence of RNDF segments that defines the mission from
     the origin to the destination.
     
     We do not need finer grain that the Route waypoints from the RNDF, since each of them is 
     implicitly linked by a cubic curve, which defines a smoother path than
     line segments would. To find the points in between the RNDF waypoints,
     use the function GetPosition.
    
     GetPosition supplements each pair of waypoints with an Hermite curve that 
     links them.
        Foley et al., Introduction to Computer Graphics.
           */
       
     /* Construct the speed profiles over the Route.
        Consider how much time we want to come up to speed, time to decelerate
        and reduced speed around turns. 
        ConstructSpeeds();  */
        
     /* Send the Route and initial speed profile to C6.
        C6 will add current vehicle position, attitude, velocity and acceleration 
        to create the vehicle state.  The state information is sent on to C3 and 
        fed back to C4.
        SendState(C6); */
}
/*---------------------------------------------------------------------------------------*/ 
/* Given a pair of waypoints and a parameter t, MakeCurve finds an intermediate waypoint
   on a smooth Hermite curve connecting them.
   0 < t < 1.
   Unfortunately, t is non linear.
   What we would really like is to know the distance from start to End along the curve,
   so that we could find a waypoint corresponding to a fraction of that distance.
   The distance along a Hermite (equivalently Bezier) curve cannot be found in closed form.
   Instead we can use t to find a sequence of points connecting start and End.
   If the straight line distance between any of these intermediate points is too large,
   subdivide with an intermediate value of t.

void MakeCurve(waypoint *start, waypoint *End, float t, waypoint *newp)
{
	double p0, p1, p2, p3;
	double q0, q1, q2, q3;
	double angle;
	double dx, dy, cos_end, sin_end;
	p0 = (double) start->east_mm;
	q0 = (double) start->north_mm;
//	angle = (90-start->bearing) * PI / 180;
	p1 = (double)(start->Evector_x1000) / 1000.;
	q1 = (double)(start->Nvector_x1000) / 1000.;
	dx = (double) End->east_mm - p0;
	dy = (double) End->north_mm - q0;
//	angle = (90-End->bearing) * PI / 180;
	cos_end = (double)(End->Evector_x1000) / 1000.;
	sin_end = (double)(End->Nvector_x1000) / 1000.;
	p2 = 3*dx - (2*p1 + cos_end);
	q2 = 3*dy - (2*q1 + sin_end);
	p3 = -2*dx + p1 + cos_end;
	q3 = -2*dy + q1 + sin_end;
	newp->east_mm  = (long) (p0 + t*(p1 + t*(p2 + t*p3)));
	newp->north_mm = (long) (q0 + t*(q1 + t*(q2 + t*q3)));
}
*/
/*---------------------------------------------------------------------------------------*/ 
/*  GetPosition is a more general interface to MakeCurve.
    Here t can be any value. The integral part counts the segments that make up the curve.

void GetPosition(curve *path, float t, waypoint *newp)
{
	curve *start = path;
	curve *End;
	newp->latitude = INVALID;
	newp->longitude = INVALID;
	newp->east_mm = 0;
	newp->north_mm = 0;
	newp->Evector_x1000 = INVALID;
	newp->Nvector_x1000 = INVALID;
	if (path->present == NULL) return;
	if (t == 0 || (t > 0 && path->next == NULL) ||
		(t < 0 && path->previous == NULL))
	{
              newp = path->present;
	      return;
	}
	while (t++ < 1)
	{
		start = start->previous;
		End =   start->previous;
		if (End == NULL)
		{
			newp =  start->present;
			return;
		}
	}
		while (t-- > 1)
	{
		start = start->next;
		End =   start->next;
		if (End == NULL)
		{
			newp =  start->present;
			return;
		}
	}
	if (t < 0)
		t = -t;
	if (t == 1)
	{
		newp =  End->present;
		return;
	}
	MakeCurve( start->present, End->present, t, newp);
}
*/
/*---------------------------------------------------------------------------------------*/ 
void setup() 
{ 
        pinMode(Rx0, INPUT);
        pinMode(Tx0, OUTPUT);
        pinMode(LED, OUTPUT); 
     	Serial.begin(9600); 
        pinMode(DATA_READY, INPUT);
        DataAvailable = false;
        attachInterrupt(0, DataReady, FALLING);

   initialize();
}
/*---------------------------------------------------------------------------------------*/ 
void loop() 
{
  /*
    Maintain a list of waypoints from current position to destination.
    Eliminate any waypoints that have already been visited.
    
    ReceiveState(C6);  // This may be at the interrupt level
    PlanPath();  // only from current position to next waypoint
    UpdateRoute();
    ConstructSpeeds();
    SendState(C6);
 
    About once a second, write the current position and remaining mission, so that if we
    get a reset, we can start from where we left off.   
    */
}


