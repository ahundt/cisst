#include <cisstRobot/robBodyBase.h>
#include <GL/gl.h>

robBodyBase::robBodyBase(){}

robBodyBase::robBodyBase( double mass,
			  const vctFixedSizeVector<double,3>& com,
			  const vctMatrixRotation3<double,VCT_ROW_MAJOR>& moit,
			  const vctFrame4x4<double,VCT_ROW_MAJOR>& Rt, 
			  const std::string& filename ){

  this->Rt = Rt;

  this->mass = mass;
  this->com = com;
  this->moit = moit;

  LoadOBJ( filename );
}

robBodyBase::~robBodyBase(){}

void robBodyBase::glMultMatrix( const vctFrame4x4<double,VCT_ROW_MAJOR>& Rt ) const {

  double H[4*4];

  H[0] = Rt[0][0]; H[4] = Rt[0][1]; H[8] = Rt[0][2]; H[12] = Rt[0][3];
  H[1] = Rt[1][0]; H[5] = Rt[1][1]; H[9] = Rt[1][2]; H[13] = Rt[1][3];
  H[2] = Rt[2][0]; H[6] = Rt[2][1]; H[10] =Rt[2][2]; H[14] = Rt[2][3];
  H[3] = 0.0;      H[7] = 0.0;      H[11] =0.0;      H[15] = 1.0;

#ifdef SINGLE_PRECISION
  glMultMatrixf(H);
#else
  glMultMatrixd(H);
#endif

}

void robBodyBase::Draw() const {

  vctFrame4x4<double,VCT_ROW_MAJOR> Rt = this->Rt;
  glMultMatrix( Rt );

  glColor3f(0.8, .8, 0.8);

  for( size_t i=0; i<ntriangles; i++ ){

    glBegin(GL_TRIANGLES);
    glNormal3d( normals[i][0], normals[i][1], normals[i][2] );
    for( size_t j=0; j<3; j++ )
      glVertex3d( vertices[ triangles[i][j] ][ 0 ], 
		  vertices[ triangles[i][j] ][ 1 ], 
		  vertices[ triangles[i][j] ][ 2 ] );
    glEnd();

  }

  Rt.InverseSelf();
  glMultMatrix( Rt );
}

bool robBodyBase::LoadOBJ( const string& filename ){

  string line;
  ifstream ifs;

  vector< vctFixedSizeVector<double,3> > vertices;
  vector< vctFixedSizeVector<int,3> > triangles;

  ifs.open(filename.data());
  if(!ifs){
    cout << "robBodyBase::LoadOBJ: " << filename << " not found." << std::endl;
    return true;
  }

  getline( ifs, line );

  while(!ifs.eof()){

    // get the 1st char of the line
    size_t first = line.find_first_not_of(" ");

    // ensure the 1st char is found (not an empty line)
    if( first != string::npos ){

      // comment line? skip the rest
      if( line.at( first ) != '#' ){

	char* array = new char[ line.size()+1 ];  // create an array
	strcpy( array, line.data() );             // copy the string
	
	char *token = strtok(array, " ");

	if( token != NULL ){
	  
	  // Vertex data
	  if( strcmp( token, "v" ) == 0 ){
	    istringstream linestream(line);
	    double x, y, z, w=1.0;
	    char v;
	    linestream >> v >> x >> y >> z >> w;
	    vertices.push_back( vctFixedSizeVector<double,3>(x, y, z) );
	  }
	  
	  if( strcmp( token, "vt" ) == 0 ){}
	  if( strcmp( token, "vn" ) == 0 ){}
	  if( strcmp( token, "vp" ) == 0 ){}
	  if( strcmp( token, "cstype" ) == 0 ){}
	  if( strcmp( token, "deg" ) == 0 ){}
	  if( strcmp( token, "bmat" ) == 0 ){}
	  if( strcmp( token, "step" ) == 0 ){}
	  
	  // Elements
	  if( strcmp( token, "p" ) == 0 ){}
	  if( strcmp( token, "l" ) == 0 ){}
	  if( strcmp( token, "f" ) == 0 ){

	    vctFixedSizeVector<int,3> triangle;     // holds the vertex indices
	    size_t n=0;

	    char* triplet = strtok(NULL, " ");
	    while( triplet ){
	      int v, vt, vn;

	      // vertex, texture, normal
	      if( sscanf( triplet, "%d/%d/%d", &v, &vt, &vn  ) == 3 ){
		if( n < 3 ){
		  triangle[n] = v-1;
		  n++;
		}
	      }

	      // vertex, normal
	      else if( sscanf( triplet, "%d//%d", &v, &vn ) == 2 ){
		if( n < 3 ){
		  triangle[n] = v-1;
		  n++;
		}
	      }
	      // vertex
	      else if( sscanf( triplet, "%d", &v ) == 1 ){
		if( n < 3 ){
		  triangle[n] = v-1;
		  n++;
		}
	      }

	      triplet = strtok(NULL, " ");
	    }

	    // did we got 3 vertices 
	    if( n == 3 ){
	      triangles.push_back( triangle );
	    }
	    else{
	      //cout<< "robBodyBase::LoadOBJ: Expected a triangle: " << line << std::endl;
	    }

	  }
	  if( strcmp( token, "curv" ) == 0 ){}
	  if( strcmp( token, "curv2" ) == 0 ){}
	  if( strcmp( token, "surf" ) == 0 ){}
	  
	  // Free-form curve/surface body statements
	  if( strcmp( token, "parm" ) == 0 ){}
	  if( strcmp( token, "trim" ) == 0 ){}
	  if( strcmp( token, "hole" ) == 0 ){}
	  if( strcmp( token, "scrv" ) == 0 ){}
	  if( strcmp( token, "sp" ) == 0 ){}
	  if( strcmp( token, "end" ) == 0 ){}
	  
	  // Connectivity between free-form surfaces
	  if( strcmp( token, "con" ) == 0 ){}
	  
	  // Grouping
	  if( strcmp( token, "g" ) == 0 ){}
	  if( strcmp( token, "s" ) == 0 ){}
	  if( strcmp( token, "mg" ) == 0 ){}
	  if( strcmp( token, "o" ) == 0 ){}
	  
	  // Display/render attributes
	  if( strcmp( token, "bevel" ) == 0 ){}
	  if( strcmp( token, "c_interp" ) == 0 ){}
	  if( strcmp( token, "d_interp" ) == 0 ){}
	  if( strcmp( token, "lod" ) == 0 ){}
	  if( strcmp( token, "usemtl" ) == 0 ){}
	  if( strcmp( token, "mtllib" ) == 0 ){}
	  if( strcmp( token, "shadow_obj" ) == 0 ){}
	  if( strcmp( token, "trace_obj" ) == 0 ){}
	  if( strcmp( token, "ctech" ) == 0 ){}
	  if( strcmp( token, "stech" ) == 0 ){}
	}
	delete[] array;
	
      }
    }
    getline( ifs, line );
  }
  
  nvertices = vertices.size();
  ntriangles = triangles.size();
  this->vertices  = new vctFixedSizeVector<double,3>[ nvertices ];
  this->triangles = new vctFixedSizeVector<int,3>[ ntriangles ];
  this->normals   = new vctFixedSizeVector<double,3>[ ntriangles ];

  for(size_t i=0; i<nvertices; i++)
    this->vertices[i] = vertices[i];

  for(size_t i=0; i<ntriangles; i++){
    this->triangles[i] = triangles[i];
    vctFixedSizeVector<double,3> v1 = vertices[ triangles[i][0] ];
    vctFixedSizeVector<double,3> v2 = vertices[ triangles[i][1] ];
    vctFixedSizeVector<double,3> v3 = vertices[ triangles[i][2] ];

    vctFixedSizeVector<double,3> n = (v2-v1)%(v3-v1);
    this->normals[i] = n / n.Norm();
  }

  return false;
}
