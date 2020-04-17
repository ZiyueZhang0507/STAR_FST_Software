#include "./FstTracking.h"

#include <iostream>
#include <fstream>
#include <cmath>

#include <TFile.h>
#include <TChain.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TMath.h>
#include <TVector3.h>

using namespace std;

ClassImp(FstTracking)

//------------------------------------------
FstTracking::FstTracking() : mList("../../list/FST/FstPed_HV140.list"), mOutPutFile("./FstPed_HV140.root")
{
  cout << "FstTracking::FstTracking() -------- Constructor!  --------" << endl;
  mHome = getenv("HOME");
}

FstTracking::~FstTracking()
{
  cout << "FstTracking::~FstTracking() -------- Release Memory!  --------" << endl;
}
//------------------------------------------
int FstTracking::Init()
{
  cout << "FstTracking::Init => " << endl;
  File_mOutPut = new TFile(mOutPutFile.c_str(),"RECREATE");

  bool isInPut = initChain(); // initialize input data/ped TChain;
  bool isTracking_Hits = initTrackingQA_Hits(); // initialize tracking with Hits
  initTracking_Hits();
  initTracking_Clusters();
  initEfficiency_Hits();
  initEfficiency_Clusters();

  if(!isInPut) 
  {
    cout << "Failed to initialize input data!" << endl;
    return -1;
  }
  if(!isTracking_Hits) 
  {
    cout << "Failed to initialize Hits based Tracking!" << endl;
    return -1;
  }

  initHitDisplay(); // initialize hit display
  initClusterDisplay(); // initialize hit display

  return 1;
}

int FstTracking::Make()
{
  cout << "FstTracking::Make => " << endl;

  long NumOfEvents = (long)mChainInPut->GetEntries();
  // if(NumOfEvents > 1000) NumOfEvents = 1000;
  // NumOfEvents = 1000;
  mChainInPut->GetEntry(0);

  std::vector<FstRawHit *> rawHitVec;
  std::vector<FstCluster *> clusterVec;
  std::vector<FstTrack *> trackHitsVec;
  std::vector<FstTrack *> trackClustersVec;
  for(int i_event = 0; i_event < NumOfEvents; ++i_event)
  {
    if(i_event%1000==0) cout << "processing events:  " << i_event << "/" << NumOfEvents << endl;
    mChainInPut->GetEntry(i_event);

    rawHitVec.clear(); // clear the container for hits
    for(int i_hit = 0; i_hit < mFstEvent_InPut->getNumRawHits(); ++i_hit)
    { // get Hits info
      FstRawHit *fstRawHit = mFstEvent_InPut->getRawHit(i_hit);
      rawHitVec.push_back(fstRawHit);
    }
    fillHitDisplay(rawHitVec); // fill Hits display
    fillTrackingQA_Hits(rawHitVec); // do tracking based on the first hit of each layer

    clusterVec.clear(); // clear the container for clusters
    for(int i_cluster = 0; i_cluster < mFstEvent_InPut->getNumClusters(); ++i_cluster)
    { // get Clusters info
      FstCluster *fstCluster = mFstEvent_InPut->getCluster(i_cluster);
      clusterVec.push_back(fstCluster);
    }
    fillClusterDisplay(clusterVec); // fill Hits display

    trackHitsVec.clear(); // clear the container for clusters
    trackClustersVec.clear(); // clear the container for clusters
    for(int i_track = 0; i_track < mFstEvent_InPut->getNumTracks(); ++i_track)
    { // get Tracks info
      FstTrack *fstTrack = mFstEvent_InPut->getTrack(i_track);
      if(fstTrack->getTrackType() == 0) // track reconstructed with hits
      {
	trackHitsVec.push_back(fstTrack);
      }
      if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
      {
	trackClustersVec.push_back(fstTrack);
      }
    }
    calResolution_Hits(mFstEvent_InPut);
    calResolution_Clusters(mFstEvent_InPut);
    calEfficiency_Hits(mFstEvent_InPut);
    calEfficiency_Clusters(mFstEvent_InPut);
  }

  cout << "processed events:  " << NumOfEvents << "/" << NumOfEvents << endl;


  return 1;
}

int FstTracking::Finish()
{
  cout << "FstTracking::Finish => " << endl;
  writeHitDisplay();
  writeClusterDisplay();
  writeTrackingQA_Hits();
  writeTracking_Hits();
  writeTracking_Clusters();
  writeEfficiency_Hits();
  writeEfficiency_Clusters();

  return 1;
}

// init Input TChain
bool FstTracking::initChain()
{
  cout << "FstTracking::initChain -> " << endl;

  mChainInPut = new TChain("mTree_FstEvent");

  if (!mList.empty())   // if input file is ok
  {
    cout << "Open input probability file list" << endl;
    ifstream in(mList.c_str());  // input stream
    if(in)
    {
      cout << "input file probability list is ok" << endl;
      char str[255];       // char array for each file name
      Long64_t entries_save = 0;
      while(in)
      {
	in.getline(str,255);  // take the lines of the file list
	if(str[0] != 0)
	{
	  string addfile;
	  addfile = str;
	  mChainInPut->AddFile(addfile.c_str(),-1,"mTree_FstEvent");
	  Long64_t file_entries = mChainInPut->GetEntries();
	  cout << "File added to data chain: " << addfile.c_str() << " with " << (file_entries-entries_save) << " entries" << endl;
	  entries_save = file_entries;
	}
      }
    }
    else
    {
      cout << "WARNING: input probability file input is problemtic" << endl;
      return false;
    }
  }

  mFstEvent_InPut = new FstEvent();
  mChainInPut->SetBranchAddress("FstEvent",&mFstEvent_InPut);

  long NumOfEvents = (long)mChainInPut->GetEntries();
  cout << "total number of events: " << NumOfEvents << endl;

  return true;
}
// init Input TChain

//--------------hit display---------------------
bool FstTracking::initHitDisplay()
{
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    std::string HistName = Form("h_mHitDisplay_Layer%d",i_layer);
    if(i_layer == 0) h_mHitDisplay[i_layer] = new TH2F(HistName.c_str(),HistName.c_str(),FST::numRStrip,-0.5,FST::numRStrip-0.5,FST::numPhiSeg,-0.5,FST::numPhiSeg-0.5);
    else h_mHitDisplay[i_layer] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::noRows,-0.5,FST::noRows-0.5);
    
    HistName = Form("h_mMaxTb_Layer%d",i_layer);
    h_mMaxTb[i_layer] = new TH1F(HistName.c_str(),HistName.c_str(),10,-0.5,9.5); 
  }

  return true;
}

void FstTracking::fillHitDisplay(std::vector<FstRawHit *> rawHitVec_orig)
{
  for(int i_hit = 0; i_hit < rawHitVec_orig.size(); ++i_hit)
  {
    int layer = rawHitVec_orig[i_hit]->getLayer();
    h_mHitDisplay[layer]->Fill(rawHitVec_orig[i_hit]->getColumn(),rawHitVec_orig[i_hit]->getRow());
    h_mMaxTb[layer]->Fill(rawHitVec_orig[i_hit]->getMaxTb());
  }
}

void FstTracking::writeHitDisplay()
{
  cout << "FstTracking::writeHitDisplay => save Hits at each Layer!" << endl;
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    h_mHitDisplay[i_layer]->Write();
    h_mMaxTb[i_layer]->Write();
  }
}
//--------------hit display---------------------

//--------------cluster display---------------------
bool FstTracking::initClusterDisplay()
{
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    std::string HistName = Form("h_mClusterDisplay_Layer%d",i_layer);
    if(i_layer == 0) h_mClusterDisplay[i_layer] = new TH2F(HistName.c_str(),HistName.c_str(),FST::numRStrip,-0.5,FST::numRStrip-0.5,FST::numPhiSeg,-0.5,FST::numPhiSeg-0.5);
    else h_mClusterDisplay[i_layer] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::noRows,-0.5,FST::noRows-0.5);
  }

  return true;
}

void FstTracking::fillClusterDisplay(std::vector<FstCluster *> clusterVec_orig)
{
  for(int i_cluster = 0; i_cluster < clusterVec_orig.size(); ++i_cluster)
  {
    int layer = clusterVec_orig[i_cluster]->getLayer();
    h_mClusterDisplay[layer]->Fill(clusterVec_orig[i_cluster]->getMeanColumn(),clusterVec_orig[i_cluster]->getMeanRow());
  }
}

void FstTracking::writeClusterDisplay()
{
  cout << "FstTracking::writeClusterDisplay => save Clusters at each Layer!" << endl;
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    h_mClusterDisplay[i_layer]->Write();
  }

}
//--------------cluster display---------------------

//--------------Track QA with Hits---------------------
bool FstTracking::initTrackingQA_Hits()
{
  const string CorrNameXR[4] = {"ist1x_ist3x","ist1x_fstR","ist3x_fstR","ist1x_ist3x_fstR"};
  const string CorrNameYPhi[4] = {"ist1y_ist3y","ist1y_fstPhi","ist3y_fstPhi","ist1y_ist3y_fstPhi"};
  for(int i_corr = 0; i_corr < 4; ++i_corr)
  { // for QA
    string HistName = Form("h_mHitsCorrXR_%s",CorrNameXR[i_corr].c_str());
    if(i_corr == 0) h_mHitsCorrXR[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::noColumns,-0.5,FST::noColumns-0.5);
    else h_mHitsCorrXR[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::numRStrip,-0.5,FST::numRStrip-0.5);

    HistName = Form("h_mHitsCorrYPhi_%s",CorrNameYPhi[i_corr].c_str());
    if(i_corr == 0) h_mHitsCorrYPhi[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noRows,-0.5,FST::noRows-0.5,FST::noRows,-0.5,FST::noRows-0.5);
    else h_mHitsCorrYPhi[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noRows,-0.5,FST::noRows-0.5,FST::numPhiSeg,-0.5,FST::numPhiSeg-0.5);
  }

  // h_mXResidual_Hits_Before = new TH1F("h_mXResidual_Hits_Before","h_mXResidual_Hits_Before",100,-500.0,500.0);
  // h_mYResidual_Hits_Before = new TH1F("h_mYResidual_Hits_Before","h_mYResidual_Hits_Before",100,-50.0,50.0);
  // h_mXResidual_Hits = new TH1F("h_mXResidual_Hits","h_mXResidual_Hits",100,-100.0,100.0);
  // h_mYResidual_Hits = new TH1F("h_mYResidual_Hits","h_mYResidual_Hits",100,-50.0,50.0);
  // h_mRResidual_Hits = new TH1F("h_mRResidual_Hits","h_mRResidual_Hits",100,-50.0,50.0);
  // h_mPhiResidual_Hits = new TH1F("h_mPhiResidual_Hits","h_mPhiResidual_Hits",100,-5.0*FST::pitchPhi,5.0*FST::pitchPhi);

  h_mXResidual_Hits_Before = new TH1F("h_mXResidual_Hits_Before","h_mXResidual_Hits_Before",100,-500.0,500.0);
  h_mYResidual_Hits_Before = new TH1F("h_mYResidual_Hits_Before","h_mYResidual_Hits_Before",100,-50.0,50.0);
  h_mXResidual_Hits = new TH1F("h_mXResidual_Hits","h_mXResidual_Hits",100,-80.0,80.0);
  h_mYResidual_Hits = new TH1F("h_mYResidual_Hits","h_mYResidual_Hits",100,-16.0,16.0);
  h_mRResidual_Hits = new TH1F("h_mRResidual_Hits","h_mRResidual_Hits",100,-80.0,80.0);
  h_mPhiResidual_Hits = new TH1F("h_mPhiResidual_Hits","h_mPhiResidual_Hits",100,-0.05,0.05);

  return true;
}

void FstTracking::fillTrackingQA_Hits(std::vector<FstRawHit *> rawHitVec_orig)
{
  int numOfHits[4]; // 0 for fst | 1-3 for ist
  std::vector<FstRawHit *> rawHitVec[4];
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    numOfHits[i_layer] = 0;
    rawHitVec[i_layer].clear();
  }

  for(int i_hit = 0; i_hit < rawHitVec_orig.size(); ++i_hit)
  { // set temp ist hit container for each layer
    int layer = rawHitVec_orig[i_hit]->getLayer();
    numOfHits[layer]++;
    rawHitVec[layer].push_back(rawHitVec_orig[i_hit]);
  }

  if(numOfHits[0] > 0 && numOfHits[1] > 0 && numOfHits[3] > 0)
  { // only do tracking when at least 1 hit is found in fst & ist1 & ist3

    // QA Histograms
    h_mHitsCorrXR[0]->Fill(rawHitVec[1][0]->getColumn(),rawHitVec[3][0]->getColumn());
    h_mHitsCorrXR[1]->Fill(rawHitVec[1][0]->getColumn(),rawHitVec[0][0]->getColumn());
    h_mHitsCorrXR[2]->Fill(rawHitVec[3][0]->getColumn(),rawHitVec[0][0]->getColumn());
    h_mHitsCorrXR[3]->Fill(5.25/2*rawHitVec[1][0]->getColumn()-3.25/2*rawHitVec[3][0]->getColumn(),rawHitVec[0][0]->getColumn());

    h_mHitsCorrYPhi[0]->Fill(rawHitVec[1][0]->getRow(),rawHitVec[3][0]->getRow());
    h_mHitsCorrYPhi[1]->Fill(rawHitVec[1][0]->getRow(),rawHitVec[0][0]->getRow());
    h_mHitsCorrYPhi[2]->Fill(rawHitVec[3][0]->getRow(),rawHitVec[0][0]->getRow());
    h_mHitsCorrYPhi[3]->Fill(5.25/2*rawHitVec[1][0]->getRow()-3.25/2*rawHitVec[3][0]->getRow()+20,rawHitVec[0][0]->getRow());

    double r_fst   = rawHitVec[0][0]->getPosX();
    double phi_fst = rawHitVec[0][0]->getPosY();
    double x0_fst  = r_fst*TMath::Cos(phi_fst); // x = r*cos(phi)
    double y0_fst  = r_fst*TMath::Sin(phi_fst); // y = r*sin(phi)
    double z0_fst  = FST::pitchLayer03;

    double x1_ist = rawHitVec[1][0]->getPosX();
    double y1_ist = rawHitVec[1][0]->getPosY();
    double z1_ist = FST::pitchLayer12 + FST::pitchLayer23;
    double x1_corr_IST = x1_ist*TMath::Cos(FST::phi_rot_ist1) + y1_ist*TMath::Sin(FST::phi_rot_ist1) + FST::x1_shift; // aligned to IST2
    double y1_corr_IST = y1_ist*TMath::Cos(FST::phi_rot_ist1) - x1_ist*TMath::Sin(FST::phi_rot_ist1) + FST::y1_shift;
    double x1_corr_FST = x1_corr_IST*TMath::Cos(FST::phi_rot_ist2) + y1_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::x2_shift; // aligned to FST
    double y1_corr_FST = y1_corr_IST*TMath::Cos(FST::phi_rot_ist2) - x1_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::y2_shift;

    double x3_ist = rawHitVec[3][0]->getPosX();
    double y3_ist = rawHitVec[3][0]->getPosY();
    double z3_ist = 0.0;
    double x3_corr_IST = x3_ist*TMath::Cos(FST::phi_rot_ist3) + y3_ist*TMath::Sin(FST::phi_rot_ist3) + FST::x3_shift; // aligned to IST2
    double y3_corr_IST = y3_ist*TMath::Cos(FST::phi_rot_ist3) - x3_ist*TMath::Sin(FST::phi_rot_ist3) + FST::y3_shift;
    double x3_corr_FST = x3_corr_IST*TMath::Cos(FST::phi_rot_ist2) + y3_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::x2_shift; // aligned to FST
    double y3_corr_FST = y3_corr_IST*TMath::Cos(FST::phi_rot_ist2) - x3_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::y2_shift;

    double x0_proj = x3_corr_IST + (x1_corr_IST-x3_corr_IST)*z0_fst/z1_ist; // before aligned to FST
    double y0_proj = y3_corr_IST + (y1_corr_IST-y3_corr_IST)*z0_fst/z1_ist;

    double x0_corr = x3_corr_FST + (x1_corr_FST-x3_corr_FST)*z0_fst/z1_ist; // after aligned to FST
    double y0_corr = y3_corr_FST + (y1_corr_FST-y3_corr_FST)*z0_fst/z1_ist;

    if(abs(rawHitVec[1][0]->getRow()-rawHitVec[3][0]->getRow()) < 17)
    {
      h_mXResidual_Hits_Before->Fill(x0_fst-x0_proj);
      h_mYResidual_Hits_Before->Fill(y0_fst-y0_proj);

      double xResidual = x0_fst-x0_corr;
      double yResidual = y0_fst-y0_corr;
      h_mXResidual_Hits->Fill(xResidual);
      h_mYResidual_Hits->Fill(yResidual);

      double r_corr = TMath::Sqrt(x0_corr*x0_corr+y0_corr*y0_corr);
      double phi_corr = TMath::ATan2(y0_corr,x0_corr);
      double rResidual = r_fst-r_corr;
      double phiResidual = phi_fst-phi_corr;
      h_mRResidual_Hits->Fill(rResidual);
      h_mPhiResidual_Hits->Fill(phiResidual);
    }
  }
}

void FstTracking::writeTrackingQA_Hits()
{
  for(int i_corr = 0; i_corr < 4; ++i_corr)
  {
    h_mHitsCorrXR[i_corr]->Write();
    h_mHitsCorrYPhi[i_corr]->Write();
  }
  h_mXResidual_Hits_Before->Write();
  h_mYResidual_Hits_Before->Write();
  h_mXResidual_Hits->Write();
  h_mYResidual_Hits->Write();
  h_mRResidual_Hits->Write();
  h_mPhiResidual_Hits->Write();
}
//--------------Track QA with Hits---------------------

//--------------Track Resolution with Hits---------------------
void FstTracking::initTracking_Hits()
{
  h_mTrackXRes_Hits   = new TH1F("h_mTrackXRes_Hits","h_mTrackXRes_Hits",100,-80.0,80.0);
  h_mTrackYRes_Hits   = new TH1F("h_mTrackYRes_Hits","h_mTrackYRes_Hits",100,-16.0,16.0);
  h_mTrackRRes_Hits   = new TH1F("h_mTrackRRes_Hits","h_mTrackRRes_Hits",100,-80.0,80.0);
  h_mTrackPhiRes_Hits = new TH1F("h_mTrackPhiRes_Hits","h_mTrackPhiRes_Hits",100,-0.05,0.05);
}

void FstTracking::calResolution_Hits(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackHitVec;
  trackHitVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < mFstEvent_InPut->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = mFstEvent_InPut->getTrack(i_track);
    if(fstTrack->getTrackType() == 0) // track reconstructed with hits
    {
      trackHitVec.push_back(fstTrack);
    }
  }

  std::vector<FstRawHit *> fstRawHitVec;
  fstRawHitVec.clear(); // clear the container for Raw Hits
  for(int i_hit = 0; i_hit < fstEvent->getNumRawHits(); ++i_hit)
  { // get Hits info
    FstRawHit *fstRawHit = fstEvent->getRawHit(i_hit);
    if(fstRawHit->getLayer() == 0 && fstRawHit->getIsHit())
    { // only use Hits
      fstRawHitVec.push_back(fstRawHit);
    }
  }
  int numOfFstHits = fstRawHitVec.size();

  if(numOfFstHits > 0)
  {
    for(int i_hit = 0; i_hit < numOfFstHits; ++i_hit)
    {
      double r_fst = fstRawHitVec[i_hit]->getPosX(); // r for fst
      double phi_fst = fstRawHitVec[i_hit]->getPosY(); // phi for fst
      double x0_fst = r_fst*TMath::Cos(phi_fst);
      double y0_fst = r_fst*TMath::Sin(phi_fst);

      // fill residual histograms
      for(int i_track = 0; i_track < trackHitVec.size(); ++i_track)
      {
	TVector3 proj_fst = trackHitVec[i_track]->getProjection(0);
	double r_proj     = proj_fst.X();
	double phi_proj   = proj_fst.Y();
	double x0_proj    = r_proj*TMath::Cos(phi_proj);
	double y0_proj    = r_proj*TMath::Sin(phi_proj);;

	double xResidual = x0_fst-x0_proj;
	double yResidual = y0_fst-y0_proj;
	h_mTrackXRes_Hits->Fill(xResidual);
	h_mTrackYRes_Hits->Fill(yResidual);

	double rResidual = r_fst-r_proj;
	double phiResidual = phi_fst-phi_proj;
	h_mTrackRRes_Hits->Fill(rResidual);
	h_mTrackPhiRes_Hits->Fill(phiResidual);
      }
    }
  }
}

void FstTracking::writeTracking_Hits()
{
  h_mTrackXRes_Hits->Write();
  h_mTrackYRes_Hits->Write();
  h_mTrackRRes_Hits->Write();
  h_mTrackPhiRes_Hits->Write();
}
//--------------Track Resolution with Hits---------------------

//--------------Track Resolution with Clusters---------------------
void FstTracking::initTracking_Clusters()
{
  h_mTrackXRes_Clusters_2Layer   = new TH1F("h_mTrackXRes_Clusters_2Layer","h_mTrackXRes_Clusters_2Layer",25,-80.0,80.0);
  h_mTrackYRes_Clusters_2Layer   = new TH1F("h_mTrackYRes_Clusters_2Layer","h_mTrackYRes_Clusters_2Layer",100,-16.0,16.0);
  h_mTrackRRes_Clusters_2Layer   = new TH1F("h_mTrackRRes_Clusters_2Layer","h_mTrackRRes_Clusters_2Layer",25,-80.0,80.0);
  h_mTrackPhiRes_Clusters_2Layer = new TH1F("h_mTrackPhiRes_Clusters_2Layer","h_mTrackPhiRes_Clusters_2Layer",100,-0.05,0.05);
  h_mTrackXResIST_2Layer         = new TH1F("h_mTrackXResIST_2Layer","h_mTrackXResIST_2Layer",15,-20.0,20.0);
  h_mTrackYResIST_2Layer         = new TH1F("h_mTrackYResIST_2Layer","h_mTrackYResIST_2Layer",100,-5.0,5.0);

  h_mTrackXRes_Clusters_3Layer   = new TH1F("h_mTrackXRes_Clusters_3Layer","h_mTrackXRes_Clusters_3Layer",25,-80.0,80.0);
  h_mTrackYRes_Clusters_3Layer   = new TH1F("h_mTrackYRes_Clusters_3Layer","h_mTrackYRes_Clusters_3Layer",100,-16.0,16.0);
  h_mTrackRRes_Clusters_3Layer   = new TH1F("h_mTrackRRes_Clusters_3Layer","h_mTrackRRes_Clusters_3Layer",25,-80.0,80.0);
  h_mTrackPhiRes_Clusters_3Layer = new TH1F("h_mTrackPhiRes_Clusters_3Layer","h_mTrackPhiRes_Clusters_3Layer",100,-0.05,0.05);
  h_mTrackXResIST_3Layer         = new TH1F("h_mTrackXResIST_3Layer","h_mTrackXResIST_3Layer",15,-20.0,20.0);
  h_mTrackYResIST_3Layer         = new TH1F("h_mTrackYResIST_3Layer","h_mTrackYResIST_3Layer",100,-5.0,5.0);
}

void FstTracking::calResolution_Clusters(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  std::vector<FstCluster *> clusterVec_fst;
  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_fst.clear();
  clusterVec_ist2.clear();

  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info for IST1 IST2 and IST3
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0)
    {
      clusterVec_fst.push_back(fstCluster);
    }
    if(fstCluster->getLayer() == 2)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  // 2-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
    {
      if(clusterVec_fst.size() > 0)
      {
	for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	{ // fill residual histograms
	  double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
	  double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
	  double x0_fst  = r_fst*TMath::Cos(phi_fst);
	  double y0_fst  = r_fst*TMath::Sin(phi_fst);

	  h_mTrackXRes_Clusters_2Layer->Fill(x0_fst-x0_proj);
	  h_mTrackYRes_Clusters_2Layer->Fill(y0_fst-y0_proj);
	  h_mTrackRRes_Clusters_2Layer->Fill(r_fst-r_proj);
	  h_mTrackPhiRes_Clusters_2Layer->Fill(phi_fst-phi_proj);
	}
      }
      if(clusterVec_ist2.size() > 0)
      {
	for(int i_cluster = 0; i_cluster < clusterVec_ist2.size(); ++i_cluster)
	{ // fill residual histograms
	  double x2_ist = clusterVec_ist2[i_cluster]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_cluster]->getMeanY(); // y for ist2

	  h_mTrackXResIST_2Layer->Fill(x2_ist-x2_proj);
	  h_mTrackYResIST_2Layer->Fill(y2_ist-y2_proj);
	}
      }
    }
  }

  // 3-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow && x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
    {
      if(clusterVec_ist2.size() > 0)
      {
	for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	{ // fill residual histograms
	  double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	  if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	  { // IST2 matching cut
	    if(clusterVec_fst.size() > 0)
	    {
	      for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	      { // fill residual histograms
		double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
		double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
		double x0_fst  = r_fst*TMath::Cos(phi_fst);
		double y0_fst  = r_fst*TMath::Sin(phi_fst);

		h_mTrackXRes_Clusters_3Layer->Fill(x0_fst-x0_proj);
		h_mTrackYRes_Clusters_3Layer->Fill(y0_fst-y0_proj);
		h_mTrackRRes_Clusters_3Layer->Fill(r_fst-r_proj);
		h_mTrackPhiRes_Clusters_3Layer->Fill(phi_fst-phi_proj);
	      }
	    }
	    h_mTrackXResIST_3Layer->Fill(x2_ist-x2_proj);
	    h_mTrackYResIST_3Layer->Fill(y2_ist-y2_proj);
	  }
	}
      }
    }
  }
}

void FstTracking::writeTracking_Clusters()
{
  h_mTrackXRes_Clusters_2Layer->Write();
  h_mTrackYRes_Clusters_2Layer->Write();
  h_mTrackRRes_Clusters_2Layer->Write();
  h_mTrackPhiRes_Clusters_2Layer->Write();
  h_mTrackXResIST_2Layer->Write();
  h_mTrackYResIST_2Layer->Write();

  h_mTrackXRes_Clusters_3Layer->Write();
  h_mTrackYRes_Clusters_3Layer->Write();
  h_mTrackRRes_Clusters_3Layer->Write();
  h_mTrackPhiRes_Clusters_3Layer->Write();
  h_mTrackXResIST_3Layer->Write();
  h_mTrackYResIST_3Layer->Write();
}
//--------------Track Resolution with Clusters---------------------

//--------------Efficiency with Hits---------------------
void FstTracking::initEfficiency_Hits()
{
  const double rMax = FST::rOuter + 5.0*FST::pitchR;
  const double rMin = FST::rOuter - 1.0*FST::pitchR;
  const double phiMax = 64.0*FST::pitchPhi;
  const double phiMin = -64.0*FST::pitchPhi;

  for(int i_match = 0; i_match < 4; ++i_match)
  {
    string HistName;
    HistName = Form("h_mTrackHits_IST_SF%d",i_match);
    h_mTrackHits_IST[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),30,rMin,rMax,128,phiMin,phiMax);
    HistName = Form("h_mTrackHits_FST_SF%d",i_match);
    h_mTrackHits_FST[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),30,rMin,rMax,128,phiMin,phiMax);
  }
}

void FstTracking::calEfficiency_Hits(FstEvent *fstEvent)
{
  const double rMax = FST::rOuter + 5.0*FST::pitchR;
  const double rMin = FST::rOuter - 1.0*FST::pitchR;
  const double phiMax = 64.0*FST::pitchPhi;
  const double phiMin = -64.0*FST::pitchPhi;

  std::vector<FstTrack *> trackHitVec;
  trackHitVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 0) // track reconstructed with hits
    {
      trackHitVec.push_back(fstTrack);
    }
  }

  std::vector<FstRawHit *> fstRawHitVec;
  fstRawHitVec.clear(); // clear the container for Raw Hits
  for(int i_hit = 0; i_hit < fstEvent->getNumRawHits(); ++i_hit)
  { // get Hits info
    FstRawHit *fstRawHit = fstEvent->getRawHit(i_hit);
    if(fstRawHit->getLayer() == 0 && fstRawHit->getIsHit())
    { // only use Hits
      fstRawHitVec.push_back(fstRawHit);
    }
  }
  int numOfFstHits = fstRawHitVec.size();

  // fill Efficiency Histograms
  for(int i_track = 0; i_track < trackHitVec.size(); ++i_track)
  {
    TVector3 proj_fst = trackHitVec[i_track]->getProjection(0);
    double r_proj = proj_fst.X();
    double phi_proj = proj_fst.Y();

    if(r_proj >= rMin && r_proj <= rMax && phi_proj >= phiMin && phi_proj <= phiMax)
    { // used for efficiency only if the projected position is within FST acceptance
      for(int i_match = 0; i_match < 4; ++i_match)
      {
	if(numOfFstHits > 0)
	{
	  for(int i_hit = 0; i_hit < numOfFstHits; ++i_hit)
	  { // loop over all possible hits
	    double r_fst = fstRawHitVec[i_hit]->getPosX();
	    double phi_fst = fstRawHitVec[i_hit]->getPosY();
	    if(i_match == 0)
	    {
	      h_mTrackHits_FST[i_match]->Fill(r_proj,phi_proj);
	    }
	    if( i_match > 0 && abs(r_fst-r_proj) <= (i_match+0.5)*FST::pitchR && abs(phi_fst-phi_proj) <= (i_match*10+0.5)*FST::pitchPhi)
	    {
	      h_mTrackHits_FST[i_match]->Fill(r_proj,phi_proj);
	    }
	    h_mTrackHits_IST[i_match]->Fill(r_proj,phi_proj);
	  }
	}
	else
	{
	  h_mTrackHits_IST[i_match]->Fill(r_proj,phi_proj);
	}
      }
    }
  }
}

void FstTracking::writeEfficiency_Hits()
{
  for(int i_match = 0; i_match < 4; ++i_match)
  {
    h_mTrackHits_IST[i_match]->Write();
    h_mTrackHits_FST[i_match]->Write();
  }
}
//--------------Efficiency with Hits---------------------

//--------------Efficiency with Clusters---------------------
void FstTracking::initEfficiency_Clusters()
{
  // const double rMax = FST::rOuter + 5.0*FST::pitchR;
  // const double rMin = FST::rOuter - 1.0*FST::pitchR;
  // const double phiMax = 64.0*FST::pitchPhi;
  // const double phiMin = -64.0*FST::pitchPhi;
  const double rMax = FST::rOuter + 4.0*FST::pitchR;
  const double rMin = FST::rOuter;
  const double phiMax = 64.0*FST::pitchPhi;
  const double phiMin = 0.0;

  for(int i_match = 0; i_match < 4; ++i_match)
  {
    string HistName;
    HistName = Form("h_mTrackClusters_IST_2Layer_SF%d",i_match);
    // h_mTrackClusters_IST_2Layer[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),30,rMin,rMax,128,phiMin,phiMax);
    h_mTrackClusters_IST_2Layer[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),20,rMin,rMax,16,phiMin,phiMax);
    HistName = Form("h_mTrackClusters_FST_2Layer_SF%d",i_match);
    h_mTrackClusters_FST_2Layer[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),20,rMin,rMax,16,phiMin,phiMax);

    HistName = Form("h_mTrackClusters_IST_3Layer_SF%d",i_match);
    h_mTrackClusters_IST_3Layer[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),20,rMin,rMax,16,phiMin,phiMax);
    HistName = Form("h_mTrackClusters_FST_3Layer_SF%d",i_match);
    h_mTrackClusters_FST_3Layer[i_match] = new TH2F(HistName.c_str(),HistName.c_str(),20,rMin,rMax,16,phiMin,phiMax);
  }
}

void FstTracking::calEfficiency_Clusters(FstEvent *fstEvent)
{
  // const double rMax = FST::rOuter + 5.0*FST::pitchR;
  // const double rMin = FST::rOuter - 1.0*FST::pitchR;
  // const double phiMax = 64.0*FST::pitchPhi;
  // const double phiMin = -64.0*FST::pitchPhi;
  const double rMax = 175;
  const double rMin = 270;
  const double phiMax = 64.0*FST::pitchPhi;
  const double phiMin = 0.02;

  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }


  std::vector<FstCluster *> clusterVec_fst;
  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_fst.clear();
  clusterVec_ist2.clear();

  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info for IST1 IST2 and IST3
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0)
    {
      clusterVec_fst.push_back(fstCluster);
    }
    if(fstCluster->getLayer() == 2)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  // fill Efficiency Histograms with 2-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
    double phi_proj = proj_fst.Y(); // r & phi for fst

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
    {
      if(r_proj >= rMin && r_proj <= rMax && phi_proj >= phiMin && phi_proj <= phiMax)
      { // used for efficiency only if the projected position is within FST acceptance
	for(int i_match = 0; i_match < 4; ++i_match)
	{
	  if(clusterVec_fst.size() > 0)
	  {
	    for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	    { // loop over all possible clusters
	      double r_fst = clusterVec_fst[i_cluster]->getMeanX();
	      double phi_fst = clusterVec_fst[i_cluster]->getMeanY();
	      if(i_match == 0)
	      {
		h_mTrackClusters_FST_2Layer[i_match]->Fill(r_proj,phi_proj);
	      }
	      if( i_match > 0 && abs(r_fst-r_proj) <= (i_match+0.5)*FST::pitchR && abs(phi_fst-phi_proj) <= (i_match*10+0.5)*FST::pitchPhi)
	      {
		h_mTrackClusters_FST_2Layer[i_match]->Fill(r_proj,phi_proj);
	      }
	      h_mTrackClusters_IST_2Layer[i_match]->Fill(r_proj,phi_proj);
	    }
	  }
	  else
	  {
	    h_mTrackClusters_IST_2Layer[i_match]->Fill(r_proj,phi_proj);
	  }
	}
      }
    }
  }

  // fill Efficiency Histograms with 3-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
    double phi_proj = proj_fst.Y(); // r & phi for fst

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position w.r.t. IST2
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow && x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
    {
      if(clusterVec_ist2.size() > 0)
      {
	for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	{ // fill residual histograms
	  double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	  if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	  { // IST2 matching cut
	    if(r_proj >= rMin && r_proj <= rMax && phi_proj >= phiMin && phi_proj <= phiMax)
	    { // used for efficiency only if the projected position is within FST acceptance
	      for(int i_match = 0; i_match < 4; ++i_match)
	      {
		if(clusterVec_fst.size() > 0)
		{
		  for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
		  { // loop over all possible clusters
		    double r_fst = clusterVec_fst[i_cluster]->getMeanX();
		    double phi_fst = clusterVec_fst[i_cluster]->getMeanY();
		    if(i_match == 0)
		    {
		      h_mTrackClusters_FST_3Layer[i_match]->Fill(r_proj,phi_proj);
		    }
		    if( i_match > 0 && abs(r_fst-r_proj) <= (i_match+0.5)*FST::pitchR && abs(phi_fst-phi_proj) <= (i_match*10+0.5)*FST::pitchPhi)
		    {
		      h_mTrackClusters_FST_3Layer[i_match]->Fill(r_proj,phi_proj);
		    }
		    h_mTrackClusters_IST_3Layer[i_match]->Fill(r_proj,phi_proj);
		  }
		}
		else
		{
		  h_mTrackClusters_IST_3Layer[i_match]->Fill(r_proj,phi_proj);
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

void FstTracking::writeEfficiency_Clusters()
{
  for(int i_match = 0; i_match < 4; ++i_match)
  {
    h_mTrackClusters_IST_2Layer[i_match]->Write();
    h_mTrackClusters_FST_2Layer[i_match]->Write();

    h_mTrackClusters_IST_3Layer[i_match]->Write();
    h_mTrackClusters_FST_3Layer[i_match]->Write();
  }
}
//--------------Efficiency with Clusters---------------------
