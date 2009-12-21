/*

  Author(s): Simon Leonard
  Created on: Nov 11 2009

  (C) Copyright 2008 Johns Hopkins University (JHU), All Rights
  Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <cisstRobot/robDH.h>

#include <iomanip>
#include <algorithm>  // for transform
#include <ctype.h>    // toupper

#include <iostream>

robDH::robDH() { alpha = a = theta = d = 0.0; }
robDH::~robDH(){}

robDHConvention robDH::DHConvention() const { return convention; }

vctFixedSizeVector<double,3> robDH::PStar() const
{ return vctFixedSizeVector<double,3>( a, d*sin(alpha), d*cos(alpha) ); }
  
vctFrame4x4<double,VCT_ROW_MAJOR> robDH::ForwardKinematics( double q ) const { 

  double d = this->d;           // copy the prismatic value
  double theta = this->theta;   // copy the revolute value

  // Add the position offset to the joint value
  switch( JointType() ){
  case robJointHinge:
    theta = PositionOffset() + q; // add the joint offset to the joint angle
    break;
  case robJointSlider:
    d = PositionOffset() + q;     // add the joint offset to the joint length
    break;
  default:
    CMN_LOG_RUN_ERROR << CMN_LOG_DETAILS
		      << ": Unsupported joint type."
		      << std::endl;
  }

  // should be computed once
  double ca = cos(this->alpha); double sa = sin(this->alpha);	
  double ct = cos(theta);       double st = sin(theta);
    
  switch( DHConvention() ){
    
    // modified DH transformation
  case robDHModified:
    {
      vctMatrixRotation3<double,VCT_ROW_MAJOR> R( ct,    -st,     0,
						  st*ca,  ct*ca, -sa,
						  st*sa,  ct*sa,  ca );
      vctFixedSizeVector<double,3> t( a, -sa*d, ca*d );
      return vctFrame4x4<double,VCT_ROW_MAJOR>( R, t );
    }
    // standard DH transformation
  case robDHStandard:
    {
      vctMatrixRotation3<double,VCT_ROW_MAJOR> R( ct, -st*ca,  st*sa,
						  st,  ct*ca, -ct*sa,
						  0,     sa,     ca );
      
      vctFixedSizeVector<double,3> t(a*ct, a*st, d);
      return vctFrame4x4<double,VCT_ROW_MAJOR>( R, t );
    }
  }
}

vctMatrixRotation3<double,VCT_ROW_MAJOR> robDH::Orientation( double q ) const {
  vctFrame4x4<double,VCT_ROW_MAJOR> Rt = ForwardKinematics( q );
  return vctMatrixRotation3<double,VCT_ROW_MAJOR>(Rt[0][0], Rt[0][1], Rt[0][2],
						  Rt[1][0], Rt[1][1], Rt[1][2],
						  Rt[2][0], Rt[2][1], Rt[2][2]);
}

robError robDH::Read( std::istream& is ) {
  std::string convention;

  is >> convention
     >> this->alpha  // read the stuff from the stream
     >> this->a 
     >> this->theta 
     >> this->d;

  // convert the convention string to a DH convention
  std::transform( convention.begin(), 
		  convention.end(), 
		  convention.begin(), 
		  toupper );

  // match the mode string to a joint mode
  if( convention.compare( "STANDARD" ) == 0 )
    this->convention = robDHStandard;
  
  else if( convention.compare( "MODIFIED" ) == 0 )
    this->convention = robDHModified;
  
  else{
    CMN_LOG_RUN_ERROR << CMN_LOG_DETAILS
		      << ": Expected a DH convention. Got " << convention << "."
		      << std::endl;
    return ERROR;
  }

  // just make sure we're accureate
  if( this->alpha == -1.5708 ) this->alpha = -M_PI_2;
  if( this->alpha ==  1.5708 ) this->alpha =  M_PI_2;

  // Read the joint parameters
  return robJoint::Read( is );
  
}

robError robDH::Write( std::ostream& os ) const {
  if( DHConvention() == robDHModified )
    os << std::setw(10) << "modified";
  else
    os << std::setw(10) << "standard";

  os << std::setw(10) << alpha 
     << std::setw(10) << a 
     << std::setw(10) << theta
     << std::setw(10) << d;

  robJoint::Write( os );
  return SUCCESS;
}
