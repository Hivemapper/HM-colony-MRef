// Copyright ETHZ 2017
// author: mathias, 2017, mathias.rothermel@googlemail.com

#include "MeshMRF.h"
#include "../LikelihoodImage/LikelihoodImage.h"
#include "../PRSTimer/PRSTimer.h"
#include "../MeshUtil/MeshGeom.h"
#include <iostream> 
#include <utility>
#include <opencv2/opencv.hpp>

// Global stuff, dirty but posssible other than that? 
static std::vector< std::vector< std::pair<int,float> > > _pairwisecostmap;
static const float PENALTYDATALABEL=0.35;
static const float PENALTYSMOOTH=0.2;

// Facade, Ground, Veg, Roof, Wat
// Smoothing cost: Potts model
float MeshMRF::Potts(int, int, int l1, int l2)
{
    //return (l1 == l2 && l1 != 0 && l2 != 0) ? 0.0f : 1.0f;
	return (l1 == l2) ? 0.0f : PENALTYSMOOTH;
}

float MeshMRF::PottsNormals(int s1, int s2 , int l1, int l2)
{
    float pwc;
    for(int i=0;i<3;i++)
    {
	if(_pairwisecostmap[s1][i].first==s2) // In avarage 2 lookups...
	{
	   	pwc=_pairwisecostmap[s1][i].second;
		break;
	}
    }
    // Now we have the angle
    // Final smoothness sould be around 0.5
    pwc=pwc/180.0*4.0;
    //std::cout<<"\n"<<pwc;
    return (l1==l2) ? pwc : 100.0*pwc;
}

float MeshMRF::pairwiseCost(MyMesh::FaceIter &f_it, MyMesh::FaceFaceIter &ff_it)
{
    MyMesh::Normal n1=_mesh->normal(*f_it);
    MyMesh::Normal n2=_mesh->normal(*ff_it);

    float angle=fabs(acos(fabs(n1[0]*n2[0]+n1[1]*n2[1]+n1[2]*n2[2])-0.000001))/3.142*180.0;
   // if(angle!=angle)std::cout<<"\n"<<angle<<" n1= " <<n1[0]<<" "<<n1[1]<<" "<<n2[2]<<" n2="<<n2[0]<<" "<<n2[1]<<" "<<n2[2]<< "res="<<n1[0]*n2[0]+n1[1]*n2[1]+n1[2]*n2[2];
    return angle;

    //return (1.0+(n1[0]*n2[0]+n1[1]*n2[1]+n1[2]*n2[2]))*0.5;
    //return (1.0+(n1[2]*n2[2]))*0.5;
}

// Data cost: differnce to one 1-x
float MeshMRF::DataOneDiff(float likelihood)
{
	return 1.0-likelihood;
}

// Data cost: differnce to one 1-x
float MeshMRF::DataLog(float likelihood)
{
	return (-1.0)*log10(likelihood);
}

// Prior data cost: for different classes define
float MeshMRF::dataPrior(int label, MyMesh::Normal normal)
{
    	float penalty=PENALTYDATALABEL;

    	float cost=0.0;

	//Facade, Ground, Veg, Roof, Wat
 	float diffangleup=fabs(acos(fabs(normal[2])-0.000001)/3.142*180.0); 
 	if(label==0)
	{
	    if( fabs(diffangleup-90.0)>30.0) cost+=penalty;
	}
 	else if(label==1)
	{
	    if( fabs(diffangleup)>30.0) cost+=penalty;
	}
 	else if(label==3)
	{
	    if( fabs(diffangleup)>60.0) cost+=penalty;
	}
 	else if(label==4)
	{
	    if( fabs(diffangleup)>20.0) cost+=penalty;
	}
	return cost;
}

MeshMRF::MeshMRF(MyMesh* mesh, MeshMetaData* mmd, const int num1abels) :
    	_mesh(mesh),
    	_numlabels(num1abels),
    	_numfaces(mesh->n_faces()),
	_rtracer(NULL),
	_lbpgraph(NULL),
	_mmd(mmd),
	_avfacesize(0)
{
	init(mesh);
	getBBCoords(mesh);
}

MeshMRF::~MeshMRF()
{
	if(_rtracer) delete _rtracer;
	if(_lbpgraph) delete _lbpgraph;
}

void MeshMRF::getBBCoords(MyMesh* mesh)
{
	float bbminx, bbmaxx, bbminy, bbmaxy, bbminz, bbmaxz;

	bbminx=_mmd->_minx-_mmd->_offsetx; bbmaxx=_mmd->_maxx-_mmd->_offsetx;
	bbminy=_mmd->_miny-_mmd->_offsety; bbmaxy=_mmd->_maxy-_mmd->_offsety;
	bbminz=_mmd->_minz-_mmd->_offsetz; bbmaxz=_mmd->_maxz-_mmd->_offsetz;

	// Fill vectors here
	_bb1 << bbminx,bbminy,bbminz,1.0;
	_bb2 << bbminx,bbminy,bbmaxz,1.0;
	_bb3 << bbminx,bbmaxy,bbminz,1.0;
	_bb4 << bbminx,bbmaxy,bbmaxz,1.0;
	_bb5 << bbmaxx,bbminy,bbminz,1.0;
	_bb6 << bbmaxx,bbminy,bbmaxz,1.0;
	_bb7 << bbmaxx,bbmaxy,bbminz,1.0;
	_bb8 << bbmaxx,bbmaxy,bbmaxz,1.0;
}

void MeshMRF::init(MyMesh* mesh)
{
	int i,j;

	// Allocate new struct
    	_facelabelcosts.clear();
    	_facelabelcosts.resize(_numfaces);
	_facepriorcosts.clear();
    	_facepriorcosts.resize(_numfaces);
    	for( i=0; i<_numfaces; i++ )
	{
		_facelabelcosts[i].resize(_numlabels);
		_facepriorcosts[i].resize(_numlabels);
		for(j=0; j<_numlabels; j++)
		{
		    _facelabelcosts[i][j]=0.0;
		    _facepriorcosts[i][j]=0.0;
		}
	}

	// Set up face hit count
	_facehitcount.clear();
    	_facehitcount.resize(_numfaces);
    	for( i=0; i<_numfaces; i++ ){ _facehitcount[i]=0; }

	// Setup face sizes
	_facesizevec.clear();
    	_facesizevec.resize(_numfaces);
	for( i=0; i<_numfaces; i++ ){ _facesizevec[i]=0.0; }

	// Setup pair-wise components 
	_pairwisecostmap.clear();
	_pairwisecostmap.resize(_numfaces);
    	for( i=0; i<_numfaces; i++ )
	{
		_pairwisecostmap[i].resize(3);
		for(j=0; j<3; j++){ _pairwisecostmap[i][j]={-1,-1.0};}
	}
}


bool MeshMRF::hitsModel(Orientation &ori)
{
    	Eigen::MatrixXd P=ori.getP();
	Eigen::Vector3d x;
	int cols=ori.cols();
	int rows=ori.rows();


	x=P*_bb1; x/=x(2);  if( ( ceil(x(0))<cols-1 && floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb2; x/=x(2);  if( ( ceil(x(0))<cols-1 && floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb3; x/=x(2); if( ( ceil(x(0))<cols-1 &&  floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb4; x/=x(2); if( ( ceil(x(0))<cols-1 &&  floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb5; x/=x(2); if( ( ceil(x(0))<cols-1 && floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb6; x/=x(2); if( ( ceil(x(0))<cols-1 && floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb7; x/=x(2); if( ( ceil(x(0))<cols-1 && floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 && floor(x(1))>0) ) { return true; }
	x=P*_bb8; x/=x(2); if( ( ceil(x(0))<cols-1 &&  floor(x(0))>0)  &&  ( ceil(x(1))<rows-1 &&  floor(x(1))>0) ) { return true; }

	return false;
}

// Get this parallel
void MeshMRF::calcDataCosts( const std::vector<std::string> &imglist, const std::vector<std::string> &orilist)
{

	int s,x,y,l,i,tid;
	int lastcols=-1; int lastrows=-1;
	int cols, rows;
	double allpercvalid=0.0;
	cv::Mat TIDimg;

	// Set up the raytracer
	_rtracer=new RayTracer(*_mesh);
	std::cout<<" line 207 meshmrf " << std::endl;
	// Skim images
	int modelcount=0;
    	for( s=0; s<imglist.size(); s++)
	{
		//std::cout<<"\nProcessing imgid ["<<s<<"]";
		PRSTimer timer;
		timer.start();
		std::cout<<" line 215 meshmrf " << std::endl;
		// Load the orientation
	    	Orientation ori(orilist[s]);
    std::cout<<" line 218 meshmrf " << std::endl;
		// Bring the guy in local mesh coordinate system
		Eigen::Vector3d C=ori.getC();
		C(0)-=_mmd->_offsetx;
		C(1)-=_mmd->_offsety;
		C(2)-=_mmd->_offsetz;
    std::cout<<" line 224 meshmrf " << std::endl;
		ori.setC(C);
    std::cout<<" line 226 meshmrf " << std::endl;

		// This is update for Agi
		//Eigen::Matrix3d R=ori.getR(); R(1,0)*=-1; R(1,1)*=-1;R(1,2)*=-1; R(2,0)*=-1; R(2,1)*=-1; R(2,2)*=-1;
		//ori.setR(R);
		//Eigen::Matrix3d K=ori.getK(); K(0,0)*=-1;
		//ori.setK(K);

		// Check if there is some intersection
		if(!hitsModel(ori))
		{
			std::cout<<"\nSkipping image: no projection" << std::endl;
		 	continue;
		}
		else
		{
        std::cout<<" line 242 meshmrf " << std::endl;
		    modelcount++;
		}

    std::cout<<" line 246 meshmrf " << std::endl;
		// Load the image
		LikelihoodImage* limage=new LikelihoodImage();
    std::cout<<" line 249 meshmrf " << std::endl;
		limage->loadImage(imglist[s]);
    std::cout<<" line 251 meshmrf " << std::endl;
		int cols=limage->cols();
		int rows=limage->rows();
		_numlabels=limage->labels();
		std::cout<<" line 255 meshmrf " << std::endl;
		//Debugstuff
		//cv::Mat out(rows,cols,CV_32F);
		//limage->getLayer(0,out);
		//std::string savename("/home/mathias/AlmostTrash/lout.png");
		//ImgIO::saveVisFloat(out,savename);
		//exit(1);

		// Check if input is sane
	    	if(cols!=ori.cols() || rows!=ori.rows()) {std::cout<<"MeshMRF.cpp: Image and ori cols/rows differ, exit."
		<< rows <<"<->"<<ori.rows()<< " "<<cols <<"<->"<< ori.cols(); exit(1);}
		std::cout<<" line 261 meshmrf " << std::endl;
		// Set the ray tracer
		_rtracer->setView(TFAR, TNEAR, ori);

	        // Process the ray tracer ... then id images are available
		_rtracer->traceRaysColumnWise();
		float percvalid=_rtracer->getPercentage();
		if(percvalid==percvalid){ allpercvalid+=percvalid; }

		TIDimg=_rtracer->getIdImage();

		//cv::Mat myimg(rows,cols,CV_8U);
		// For each valid pixel hit its triangle
		for( y=0; y<rows; y++)
		{
			for( x=0; x<cols; x++)
	    		{
				tid=TIDimg.at<int>(y,x);

				if(tid!=-1)
				{
					// Skim the stack of Likelihood images 
    					for( l=0; l<_numlabels; l++)
     					{
						// Get the cost	
					    	//float cost=DataOneDiff(limage->getData(y,x,l));
						_facelabelcosts[tid][l]+=limage->getData(y,x,l);
					}
					_facehitcount[tid]++;
				}
	    		}
    		}
		delete limage;
	}
	std::cout<<"\nAv. size of model projection "<<float(floor(allpercvalid*10000/modelcount))/100.0<<"%.";
	delete _rtracer; _rtracer=NULL;
}

void MeshMRF::calcDataCostPrior()
{
	_mesh->request_face_normals();
	_mesh->update_normals();
	int f=0;
	MyMesh::Normal n;
	for (MyMesh::FaceIter f_it=_mesh->faces_begin(); f_it!=_mesh->faces_end(); ++f_it)
	{
		n=_mesh->normal(*f_it);
		_avfacesize+=_facesizevec[f]=MeshGeom::faceArea(f_it,_mesh);
	    	for(int l=0;l<_numlabels;l++)
	    	{
			_facepriorcosts[f][l]=dataPrior(l,n);
		}
		f++;
	}
	_avfacesize/=(double)f;
}

// Fills the graph struct
void MeshMRF::fillGraph()
{

	_mesh->request_face_normals();
	_mesh->update_normals();
    	int n,i;

	// Set edges of graph Skiming face ... ids are linear!!! at least if nothing is inserted
	int m=0;
	for (MyMesh::FaceIter f_it=_mesh->faces_begin(); f_it!=_mesh->faces_end(); ++f_it)
	{
	    	int i=0;
    		for(MyMesh::FaceFaceIter ff_it=_mesh->ff_iter(*f_it); ff_it.is_valid(); ++ff_it)
		{
		    	n=ff_it->idx();
			_pairwisecostmap[m][i].first=n;
			_pairwisecostmap[m][i++].second=pairwiseCost(f_it,ff_it);
		    	_lbpgraph->set_neighbors_single(m,n++); // Be careful... this sets both directions between graph ...
		}
		m++;
	}

	// Now we set the costs... change this copy mess
	std::vector<mrf::SparseDataCost> costvec(_numfaces);
	for(int l=0; l<_numlabels; l++)
	{
		for(int f=0;f<_numfaces;f++)
		{
		    	costvec[f].site=f;
			if(_facehitcount[f]>0)
			{
			      	//costvec[f].cost=DataOneDiff(_facelabelcosts[f][l]/(float)_facehitcount[f]);
		    		costvec[f].cost=DataLog(_facelabelcosts[f][l]/(float)_facehitcount[f]);
				// Add the data prior
				costvec[f].cost+=_facepriorcosts[f][l];
				costvec[f].cost*=_facesizevec[f]/_avfacesize;
			}
			else
			{
				costvec[f].cost=1.0/(float)_numlabels;
			}
		}
		_lbpgraph->set_data_costs(l,costvec);
	}
	_mesh->release_face_normals();
}

void MeshMRF::assignMinLabels()
{
    	int f=0;
	for (MyMesh::FaceIter f_it=_mesh->faces_begin(); f_it!=_mesh->faces_end(); ++f_it)
	{
	    _mesh->data(*f_it).setlabelid(_lbpgraph->what_label(f)); f++;
	}
}

void MeshMRF::process(const std::vector< std::string> &imglist, const std::vector< std::string > &orilist, const int maxiterations)
{
	// For each face compute the data cost of each label
    	std::cout<<"\nAssigning costs to faces...";
    	PRSTimer timer; timer.start();
    	calcDataCosts( imglist, orilist);
	calcDataCostPrior();
	timer.stop();
	std::cout<< " done." << timer.getTimeSec();

	// Bootstrap the graph...
	timer.reset(); timer.start();
	std::cout<<"\nBoostrapping graph (faces="<<_numfaces<<"|labels="<<_numlabels<<")... ";
	_lbpgraph=new mrf::LBPGraph(_numfaces,_numlabels);
	fillGraph();
	_lbpgraph->set_smooth_cost(&Potts);
	timer.stop();
	std::cout<< " done." << timer.getTimeSec();

	// Optimization
	timer.reset(); timer.start();
	std::cout<<"\nOptimization ...";
	float energy = 0;
	for (int i=0;i<maxiterations;i++) {
		energy = _lbpgraph->optimize(1);
		std::cout<< "testing...  ";
		std::cout<< "Energy_opt_e6 = " << energy*0.000001 << std::endl;
	}
	timer.stop();
	std::cout<<"\n Energy = "<< energy << std::endl;
	//std::cout<< " done." << timer.getTimeSec();

	// Assign resulting face labels to graph
	assignMinLabels();

	delete _lbpgraph; _lbpgraph=NULL;
}


