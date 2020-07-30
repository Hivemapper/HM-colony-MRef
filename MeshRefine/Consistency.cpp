#include "MeshRefine.h"
#include "GradCalcStereo.h"
#include "GradThinPlate.h"
#include "GradStraightEdges.h"
#include "../MeshClassify/MeshMRF.h"
#include "../LikelihoodImage/LikelihoodImage.h"
#include "../PRSTimer/PRSTimer.h"
#include "../MeshUtil/MeshDivide.h"
#include "../MeshUtil/MeshIO.h"
#include "../FileSystemUtil/FDUtil.h"

#include <iostream>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <boost/filesystem.hpp>

static void showUsage(std::string name )
{
  std::cerr << "Usage: " <<  name << " <option(s)> SOURCES"
      << "Options:\n"
      << "\t-h,--help\t\tShow this help message\n"
      << "\t-b,--basepath \t\tSpecify the basepath \n"
      << "\t-st,--starttile \t\tSpecify the starttile id \n"
      << "\t-et,--endtile \t\tSpecify the endtile id \n";
} 


static bool  parseArguments(  int argc, char* argv[],
        std::string& basepath,
        int& starttile,
        int& endtile)
{
  int nt=-1;
  basepath="";
  starttile=-1;
  endtile=-1;

  for(int i=1; i<argc; ++i)
  {
      std::string arg = argv[i];
    if ((arg == "-h") || (arg == "--help"))
    {
      showUsage(argv[0]);
      return 0;
    }
    else if ((arg == "-b") || (arg == "--basepath"))
    {
      if (i + 1 < argc){ basepath=argv[++i]; }
      else
      {
        std::cerr << "--basepath option requires argument.\n";
              return 1;
      }
    }
    else if ((arg == "-st") || (arg == "--starttile"))
    {
      if (i + 1 < argc){ starttile=atoi(argv[++i]); }
      else
      {
        std::cerr << "--starttile option requires argument.\n";
              return 1;
      }
    }
    else if ((arg == "-et") || (arg == "--endtile"))
    {
      if (i + 1 < argc){ endtile=atoi(argv[++i]); }
      else
      {
        std::cerr << "--starttile option requires argument.\n";
              return 1;
      }
    }
  }

  //Verify
  if(basepath=="") { std::cout<<"\nBasepath required."; showUsage(argv[0]); exit(1); }

  return true;
}

void verifyInput(const IOList& imglist, const IOList& orilist, const IOList& likelilist, const IOList& meshlist, const ControlRefine& controlrefine)
{

      // Check proper sizes
  if(imglist.size()<=1){std::cout<<"\nImglist: size=1/0."; exit(1);}
  if(orilist.size()<=1){std::cout<<"\nOrilist: size=1/0."; exit(1);}
  if(likelilist.size()<=1){std::cout<<"\nLiklilist: size=1/0.";exit(1);}
  if(meshlist.size()<1){std::cout<<"\nMeshlist: size=1/0."; exit(1);}

  // Check sizes
  int imgnum=imglist.size();
  if(orilist.size()!=imgnum) {std::cout<<"Number of oris different from number of imgs"; exit(1);}
  if(controlrefine._usesemanticdata || controlrefine._usesemanticsmooth)
  {
      if(likelilist.size()!=imgnum) {std::cout<<"Number of likeliimgs different from number of imgs"; exit(1); }
  }
}

void assembleTileList(std::vector<int>& tilestoprocess, const int starttile, const int endtile, const int numalltiles)
{
  // Update meshlist
  if( starttile==-1 || endtile==-1 )
  {
    tilestoprocess.resize(numalltiles);
    for(int i=0;i<numalltiles;i++)
    {
      tilestoprocess[i]=i;
    }
  }
  else // Verify if all in range
  {
        if(starttile>endtile){ std::cout<<"\nError: startile>endtile"; exit(1); }
    else if (starttile<0){ std::cout<<"\nError: startile<0"; exit(1); }
    else if (endtile>=numalltiles){ std::cout<<"\nError: endtile>numalltiles"; exit(1); }
    else
    {
      int count=0;  
          tilestoprocess.resize(endtile-starttile+1);
      for(int i=starttile;i<=endtile;i++)
      {
          tilestoprocess[count++]=i;
      }
    }
  }
}

void prepareOutput(const std::string basepath, const IOList& tilelist, const std::vector<int>& tilenum, std::vector<std::string>& outfilenames)
{
  boost::filesystem::path fsbasepath(basepath);
  if(!boost::filesystem::exists(fsbasepath))
  {
        std::cout<<"Basepath does not exist.";
    exit(1);
  }

  // Make the output folder
  std::string outdir(basepath);
  outdir.append("/out");
  boost::filesystem::path fsoutpath(outdir);
  if(!boost::filesystem::exists(fsoutpath))
  {
    boost::filesystem::create_directory(fsoutpath);
  }

  // Make the new output files
  for(int i=0; i<tilenum.size(); i++)
  {
        std::string filename=basepath;
    filename.append("/out/");
    filename.append(tilelist.getNameWithoutEnding(tilenum[i]));
    filename.append(".obj");
    outfilenames.push_back(filename);
  }
}


void printSummary(const std::vector<int>& tilestoprocess, const std::vector<std::string>& outfilenames, const IOList& tilelist)
{
      for(int t=0;t<tilestoprocess.size();t++)
  {
      std::cout<<"\n\n Processing tile: "<<tilestoprocess[t]<< " tile" << tilelist.getNameWithoutEnding(tilestoprocess[t]);
      std::cout<<"\n Savename is: "<< outfilenames[t] <<"\n\n";
  }
}


int main(int argc, char* argv[]){
    // Parse Input
    std::string basepath;
  int starttile,endtile;
    parseArguments(argc, argv, basepath, starttile, endtile);

    std::string imglistname=basepath; imglistname.append("/imglist.txt");
    std::string likelilistname=basepath; likelilistname.append("/likelilist.txt"); // Can be blank, not doing semantic yet
    std::string orilistname=basepath; orilistname.append("/orilist.txt");
    std::string meshlistname=basepath; meshlistname.append("/meshlist.txt");
  std::string controlfilename=basepath; controlfilename.append("/ControlRefine.txt");
  IOList imglist(imglistname);
  IOList likelilist(likelilistname);
  IOList orilist(orilistname);
  IOList meshlist(meshlistname);

    // Read Controlfile
  ControlRefine ctr;
  if(!boost::filesystem::exists(controlfilename.c_str()))
  {
      ctr.writeFile(controlfilename);
      std::cout<<"\n\nNo controlfile found, new one generated at:"<< controlfilename <<". Modify and rerun.\n\n";
      return 1;
  }
  else
  {
    ctr.readFile(controlfilename);
    ctr.writeFile(controlfilename); // To cope with modified control file structure
  }

  // Should be enhanced at some point
  verifyInput(imglist, orilist, likelilist, meshlist, ctr);

    // Verify the list of tiles to be processed
  std::vector<int> tilestoprocess;
  assembleTileList(tilestoprocess, starttile, endtile, meshlist.size());

  // Preparing output folder and output tile names
  std::vector<std::string> outfilenames;
  prepareOutput(basepath, meshlist, tilestoprocess, outfilenames);

  printSummary(tilestoprocess,outfilenames,meshlist);

  MyMesh mesh; // mesh with original vertices
  std::string meshname(meshlist.getElement(0));
  MeshIO::readMesh(mesh, meshname, true, false);
  int numverts = mesh.n_vertices();

  // Read mesh meta data
  FDUtil::exchangeExtension(meshname,"mmd");
  MeshMetaData mmd(meshname);

    //  *** Photometric and Semantic Consistency ***
    // Boostrap full adjancy matrix
  Eigen::MatrixXf adjacency(orilist.size(),orilist.size());
  adjacency<<   0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
      1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
      1,1,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
      1,1,1,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
      1,1,1,1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
      1,1,1,1,1,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
      1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
      1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
      1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
      1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,
      0,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,
      0,0,0,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,
      0,0,0,0,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,0,0,0,
      0,0,0,0,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,0,0,
      0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,0,
      0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,1,1,1,1,0,0,
      0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,1,1,1,1,0,
      0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,1,1,1,1,
      0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,1,1,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,1,1,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,1,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0;

    MeshRefine refmod(&mesh, &mmd, &ctr, &adjacency, &imglist, &likelilist, &orilist);
   refmod.process();

  std::cout<<"\nSaving..."<<outfilenames[0] << std::endl;
  mesh.request_face_colors();
  mesh.request_vertex_colors();
  MeshConv::faceLabelToFaceColorICCV(mesh);
  std::cout<<" consistency.cpp: line 261" << std::endl;
  MeshIO::writeMesh(mesh, outfilenames[0],true,true, false, false);
//  void writeMesh( const MyMesh &mesh, const std::string &savename, const bool hasvertcolor ,const bool hasfacecolor,const bool hasvertnormal=false, const bool hasfacetexture=false );
  MeshIO::writeMesh(mesh, "data2/out/segmentation_mesh_n_asc_vert.ply",true,true, false, false);
  std::cout<<"..Done";


    return 0;

}