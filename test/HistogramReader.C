#include "HistogramReader.h"

using namespace std;


//------------------------------------------------------------------------------
// HistogramReader
//------------------------------------------------------------------------------
HistogramReader::HistogramReader(const TString& inputdir,
				 const TString& outputdir) :
  _inputdir     (inputdir),
  _outputdir    (outputdir),
  _stackoption  ("nostack,hist"),
  _title        ("inclusive"),
  _luminosity_fb(-1),
  _datanorm     (false),
  _drawratio    (false),
  _drawyield    (false),
  _publicstyle  (false),
  _savepdf      (false),
  _savepng      (true)
{
  _mcfile.clear();
  _mccolor.clear();
  _mclabel.clear();
  _mcscale.clear();

  _datafile  = NULL;
  _datahist  = NULL;
  _allmchist = NULL;

  TH1::SetDefaultSumw2();
}


//------------------------------------------------------------------------------
// AddData
//------------------------------------------------------------------------------
void HistogramReader::AddData(const TString& filename,
			      const TString& label,
			      Color_t        color)
{
  TString fullname = _inputdir + "/" + filename + ".root";

  if (gSystem->AccessPathName(fullname))
    {
      printf(" [HistogramReader::AddData] Cannot access %s\n", fullname.Data());
      return;
    }

  TFile* file = new TFile(fullname, "update");

  _datafile  = file;
  _datalabel = label;
  _datacolor = color;
}


//------------------------------------------------------------------------------
// AddProcess
//------------------------------------------------------------------------------
void HistogramReader::AddProcess(const TString& filename,
				 const TString& label,
				 Color_t        color,
				 Int_t          kind,
				 Float_t        scale)
{
  TString fullname = _inputdir + "/" + filename + ".root";

  if (gSystem->AccessPathName(fullname))
    {
      printf(" [HistogramReader::AddProcess] Cannot access %s\n", fullname.Data());
      return;
    }

  TFile* file = new TFile(fullname, "update");

  _mcfile.push_back(file);
  _mclabel.push_back(label);
  _mccolor.push_back(color);
  _mcscale.push_back(scale);
  
  if (scale > 0. && scale != 1.)
    printf("\n [HistogramReader::AddProcess] Process %s will be scaled by %.2f\n\n", label.Data(), scale);

  if (kind == roc_signal)     _roc_signals    .push_back(fullname);
  if (kind == roc_background) _roc_backgrounds.push_back(fullname);
}


//------------------------------------------------------------------------------
// AddSignal
//------------------------------------------------------------------------------
void HistogramReader::AddSignal(const TString& filename,
				const TString& label,
				Color_t        color,
				Int_t          kind)
{
  TString fullname = _inputdir + "/" + filename + ".root";
  
  if (gSystem->AccessPathName(fullname))
    {
      printf(" [HistogramReader::AddSignal] Cannot access %s\n", fullname.Data());
      return;
    }

  TFile* file = new TFile(fullname, "update");

  _signalfile.push_back(file);
  _signallabel.push_back(label);
  _signalcolor.push_back(color);

  if (kind == roc_signal)     _roc_signals    .push_back(fullname);
  if (kind == roc_background) _roc_backgrounds.push_back(fullname);
}


//------------------------------------------------------------------------------
// Draw
//------------------------------------------------------------------------------
void HistogramReader::Draw(TString hname,
			   TString xtitle,
			   Int_t   ngroup,
			   Int_t   precision,
			   TString units,
			   Bool_t  setlogy,
			   Bool_t  moveoverflow,
			   Float_t xmin,
			   Float_t xmax,
			   Float_t ymin,
			   Float_t ymax)
{
  TString cname = hname;

  if (_stackoption.Contains("nostack")) cname += "_nostack";

  if (setlogy) cname += "_log";

  _writeyields = (hname.Contains("_evolution")) ? true : false;

  if (_writeyields)
    {
      _yields_table.open(_outputdir + "/" + cname + ".txt");

      _writelabels = true;
    }


  TCanvas* canvas = NULL;

  TPad* pad1 = NULL;
  TPad* pad2 = NULL;

  if (_drawratio && _datafile)
    {
      canvas = new TCanvas(cname, cname, 550, 720);

      pad1 = new TPad("pad1", "pad1", 0, 0.3, 1, 1.0);
      pad2 = new TPad("pad2", "pad2", 0, 0.0, 1, 0.3);

      pad1->SetTopMargin   (0.08);
      pad1->SetBottomMargin(0.02);
      pad1->Draw();

      pad2->SetTopMargin   (0.08);
      pad2->SetBottomMargin(0.35);
      pad2->Draw();
    }
  else
    {
      canvas = new TCanvas(cname, cname, 550, 600);

      pad1 = new TPad("pad1", "pad1", 0, 0, 1, 1);

      pad1->Draw();
    }


  //----------------------------------------------------------------------------
  // pad1
  //----------------------------------------------------------------------------
  pad1->cd();
  
  pad1->SetLogy(setlogy);


  // Stack processes
  //----------------------------------------------------------------------------
  _mchist.clear();

  THStack* mcstack = new THStack(hname + "_mcstack", hname + "_mcstack");

  for (UInt_t i=0; i<_mcfile.size(); i++) {

    _mcfile[i]->cd();

    TH1D* dummy = (TH1D*)_mcfile[i]->Get(hname);

    _mchist.push_back((TH1D*)dummy->Clone());

    if (_luminosity_fb > 0 && _mcscale[i] > -999) _mchist[i]->Scale(_luminosity_fb);

    if (_mcscale[i] > 0) _mchist[i]->Scale(_mcscale[i]);

    SetHistogram(_mchist[i], _mccolor[i], 1001, kDot, kSolid, 0, ngroup, moveoverflow, xmin, xmax);
    
    mcstack->Add(_mchist[i]);
  }


  // Stack signals
  //----------------------------------------------------------------------------
  _signalhist.clear();

  THStack* signalstack = new THStack(hname + "_signalstack", hname + "_signalstack");

  for (UInt_t i=0; i<_signalfile.size(); i++) {

    _signalfile[i]->cd();

    TH1D* dummy = (TH1D*)_signalfile[i]->Get(hname);

    _signalhist.push_back((TH1D*)dummy->Clone());

    if (_luminosity_fb > 0) _signalhist[i]->Scale(_luminosity_fb);

    SetHistogram(_signalhist[i], _signalcolor[i], 0, kDot, kSolid, 3, ngroup, moveoverflow, xmin, xmax);
    
    signalstack->Add(_signalhist[i]);
  }


  // Get the data
  //----------------------------------------------------------------------------
  if (_datafile)
    {
      _datafile->cd();

      TH1D* dummy = (TH1D*)_datafile->Get(hname);

      _datahist = (TH1D*)dummy->Clone();
      
      SetHistogram(_datahist, kBlack, 0, kFullCircle, kSolid, 1, ngroup, moveoverflow, xmin, xmax);
    }


  // Normalize MC to data
  //----------------------------------------------------------------------------
  if (_datahist && _datanorm)
    {
      Float_t mcnorm   = Yield((TH1D*)(mcstack->GetStack()->Last()));
      Float_t datanorm = Yield(_datahist);

      for (UInt_t i=0; i<_mchist.size(); i++)
	{
	  _mchist[i]->Scale(datanorm / mcnorm);
	}

      mcstack->Modified();
    }


  // hfirst will contain the axis settings
  //----------------------------------------------------------------------------
  TH1D* hfirst = (TH1D*)_mchist[0]->Clone("hfirst");

  hfirst->Reset();


  // All MC
  //----------------------------------------------------------------------------
  _allmchist = (TH1D*)_mchist[0]->Clone("allmchist");

  _allmchist->SetName(_mchist[0]->GetName());

  // Possible modification (how to deal with systematic uncertainties?)
  //  _allmchist = (TH1D*)(mcstack->GetStack()->Last());

  for (Int_t ibin=0; ibin<=_allmchist->GetNbinsX(); ibin++) {

    Float_t binValue = 0.;
    Float_t binError = 0.;

    for (UInt_t i=0; i<_mchist.size(); i++) {

      Float_t binContent   = _mchist[i]->GetBinContent(ibin);
      Float_t binStatError = _mchist[i]->GetBinError(ibin);
      Float_t binSystError = 0;  // To be updated
      
      binValue += binContent;
      binError += (binStatError * binStatError);
      binError += (binSystError * binSystError);
    }
    
    binError = sqrt(binError);

    _allmchist->SetBinContent(ibin, binValue);
    _allmchist->SetBinError  (ibin, binError);
  }

  _allmclabel = "stat";

  _allmchist->SetFillColor  (kGray+1);
  _allmchist->SetFillStyle  (   3345);
  _allmchist->SetLineColor  (kGray+1);
  _allmchist->SetMarkerColor(kGray+1);
  _allmchist->SetMarkerSize (      0);


  // Draw
  //----------------------------------------------------------------------------
  hfirst->Draw();

  mcstack->Draw(_stackoption + ",same");

  if (!_stackoption.Contains("nostack")) _allmchist->Draw("e2,same");

  if (_signalfile.size() > 0) signalstack->Draw("nostack,hist,same");

  if (_datahist) _datahist->Draw("ep,same");


  // Set xtitle and ytitle
  //----------------------------------------------------------------------------
  TString ytitle = Form("events / %s.%df", "%", precision);

  ytitle = Form(ytitle.Data(), hfirst->GetBinWidth(0));

  if (!units.Contains("NULL")) {
    xtitle = Form("%s [%s]", xtitle.Data(), units.Data());
    ytitle = Form("%s %s",   ytitle.Data(), units.Data());
  }


  // Adjust xaxis and yaxis
  //----------------------------------------------------------------------------
  hfirst->GetXaxis()->SetRangeUser(xmin, xmax);

  Float_t theMin = 0.0;

  Float_t theMax = (_datahist) ? GetMaximum(_datahist, xmin, xmax) : 0.0;

  Float_t theMaxMC = GetMaximum(_allmchist, xmin, xmax);

  if (_stackoption.Contains("nostack"))
    {
      for (UInt_t i=0; i<_mcfile.size(); i++)
	{
	  Float_t mchist_i_max = GetMaximum(_mchist[i], xmin, xmax, false);

	  if (mchist_i_max > theMaxMC) theMaxMC = mchist_i_max;
	}
    }

  if (theMaxMC > theMax) theMax = theMaxMC;

  Float_t theMaxSignal = 0.0;

  if (_signalfile.size() > 0)
    {
      for (UInt_t i=0; i<_signalfile.size(); i++)
	{
	  Float_t signalhist_i_max = GetMaximum(_signalhist[i], xmin, xmax, false);

	  if (signalhist_i_max > theMaxSignal) theMaxSignal = signalhist_i_max;
	}
    }

  if (theMaxSignal > theMax) theMax = theMaxSignal;

  if (pad1->GetLogy())
    {
      theMin = 1e-5;
      theMax = TMath::Power(10, TMath::Log10(theMax) + 6);
    }
  else if (!_stackoption.Contains("nostack"))
    {
      theMax *= 1.7;
    }

  hfirst->SetMinimum(theMin);
  hfirst->SetMaximum(theMax);

  if (ymin != -999) hfirst->SetMinimum(ymin);
  if (ymax != -999) hfirst->SetMaximum(ymax);


  // Legend
  //----------------------------------------------------------------------------
  Float_t x0     = 0.220;                         // x position of the data on the top left
  Float_t y0     = 0.843;                         // y position of the data on the top left
  Float_t xdelta = (_drawyield) ? 0.228 : 0.170;  // x width between columns
  Float_t ydelta = 0.050;                         // y width between rows
  Int_t   nx     = 0;                             // column number
  Int_t   ny     = 0;                             // row    number

  TString opt = (_stackoption.Contains("nostack")) ? "l" : "f";


  // Data legend
  //----------------------------------------------------------------------------
  if (_datahist)
    {
      DrawLegend(x0, y0, _datahist, _datalabel.Data(), "lp");
      ny++;
    }


  // All MC legend
  //----------------------------------------------------------------------------
  if (!_stackoption.Contains("nostack"))
    {
      DrawLegend(x0, y0 - ny*ydelta, _allmchist, _allmclabel.Data(), opt);
      ny++;
    }


  // Standard Model processes legend
  //----------------------------------------------------------------------------
  Int_t nrow = (_mchist.size() > 10) ? 5 : 4;

  for (int i=0; i<_mchist.size(); i++)
    {
      if (ny == nrow)
	{
	  ny = 0;
	  nx++;
	}

      DrawLegend(x0 + nx*xdelta, y0 - ny*ydelta, _mchist[i], _mclabel[i].Data(), opt);
      ny++;
    }
  
  
  // Search signals legend in a new column
  //----------------------------------------------------------------------------
  ny = 0;
  nx++;

  for (int i=0; i<_signalhist.size(); i++)
    {
      DrawLegend(x0 + nx*xdelta, y0 - ny*ydelta, _signalhist[i], _signallabel[i].Data(), "l");
      ny++;
    }


  // Titles
  //----------------------------------------------------------------------------
  Float_t xprelim = (_drawratio && _datafile) ? 0.288 : 0.300;

  if (_title.EqualTo("inclusive"))
    {
      DrawLatex(61, 0.190,   0.945, 0.050, 11, "CMS");
      DrawLatex(52, xprelim, 0.945, 0.030, 11, "Preliminary");
    }
  else
    {
      DrawLatex(42, 0.190, 0.945, 0.050, 11, _title);
    }

  if (_luminosity_fb > 0)
    DrawLatex(42, 0.940, 0.945, 0.050, 31, Form("%.3f fb^{-1} (13TeV)", _luminosity_fb));
  else
    DrawLatex(42, 0.940, 0.945, 0.050, 31, "(13TeV)");

  SetAxis(hfirst, xtitle, ytitle, 1.5, 1.8);


  //----------------------------------------------------------------------------
  // pad2
  //----------------------------------------------------------------------------
  if (_drawratio && _datafile)
    {
      pad2->cd();

      // This approach isn't yet working
      //      TGraphAsymmErrors* g = new TGraphAsymmErrors();
      //      g->Divide(_datahist, _allmchist, "cl=0.683 b(1,1) mode");
      //      g->SetMarkerStyle(kFullCircle);
      //      g->Draw("ap");

      TH1D* ratio       = (TH1D*)_datahist ->Clone("ratio");
      TH1D* uncertainty = (TH1D*)_allmchist->Clone("uncertainty");

      for (Int_t ibin=1; ibin<=ratio->GetNbinsX(); ibin++) {

	Float_t dtValue = _datahist->GetBinContent(ibin);
	Float_t dtError = _datahist->GetBinError  (ibin);

	Float_t mcValue = _allmchist->GetBinContent(ibin);
	Float_t mcError = _allmchist->GetBinError  (ibin);

	Float_t ratioVal         = 999;
	Float_t ratioErr         = 999;
	Float_t uncertaintyError = 999;

	if (mcValue > 0)
	  {
	    ratioVal         = dtValue / mcValue;
	    ratioErr         = dtError / mcValue;
	    uncertaintyError = ratioVal * mcError / mcValue;
	  }
	
	ratio->SetBinContent(ibin, ratioVal);
	ratio->SetBinError  (ibin, ratioErr);
	
	uncertainty->SetBinContent(ibin, 1.);
	uncertainty->SetBinError  (ibin, uncertaintyError);
      }

      ratio->Draw("ep");

      ratio->GetXaxis()->SetRangeUser(xmin, xmax);
      ratio->GetYaxis()->SetRangeUser(0.7, 1.3);

      uncertainty->Draw("e2,same");

      ratio->Draw("ep,same");

      SetAxis(ratio, xtitle, "data / MC", 1.4, 0.75);
    }


  //----------------------------------------------------------------------------
  // Save it
  //----------------------------------------------------------------------------
  canvas->cd();

  if (_savepdf) canvas->SaveAs(_outputdir + cname + ".pdf");
  if (_savepng) canvas->SaveAs(_outputdir + cname + ".png");

  if (_writeyields)
    {
      _yields_table << std::endl;
      
      _yields_table.close();
    }
}


//------------------------------------------------------------------------------
// CrossSection
//------------------------------------------------------------------------------
void HistogramReader::CrossSection(TString level,
				   TString channel,
				   TString process,
				   Float_t branchingratio,
				   TString signal1_filename,
				   Float_t signal1_xs,
				   Float_t signal1_ngen,
				   TString signal2_filename,
				   Float_t signal2_xs,
				   Float_t signal2_ngen)
{
  if (_luminosity_fb < 0)
    {
      printf("\n [HistogramReader::CrossSection] Warning: reading negative luminosity\n\n");
    }


  // Get the signal (example qqWW)
  //----------------------------------------------------------------------------
  TFile* signal1_file = new TFile(_inputdir + "/" + signal1_filename + ".root");

  float signal1_counterLum = Yield((TH1D*)signal1_file->Get(level + "/h_counterLum_" + channel));
  float signal1_counterRaw = Yield((TH1D*)signal1_file->Get(level + "/h_counterRaw_" + channel));

  float counterSignal = signal1_counterLum * _luminosity_fb;

  float efficiency = signal1_counterRaw / signal1_ngen;


  // Get the second signal (example ggWW)
  //----------------------------------------------------------------------------
  if (!signal2_filename.Contains("NULL"))
    {
      TFile* signal2_file = new TFile(_inputdir + "/" + signal2_filename + ".root");

      float signal2_counterLum = Yield((TH1D*)signal2_file->Get(level + "/h_counterLum_" + channel));
      float signal2_counterRaw = Yield((TH1D*)signal2_file->Get(level + "/h_counterRaw_" + channel));

      counterSignal += (signal2_counterLum * _luminosity_fb);

      float signal1_fraction = signal1_xs / (signal1_xs + signal2_xs);
      float signal2_fraction = 1. - signal1_fraction;

      float signal1_efficiency = signal1_counterRaw / signal1_ngen;
      float signal2_efficiency = signal2_counterRaw / signal2_ngen;

      efficiency = signal1_fraction*signal1_efficiency + signal2_fraction*signal2_efficiency;
    }


  // Get the backgrounds
  //----------------------------------------------------------------------------
  float counterBackground = 0;

  for (UInt_t i=0; i<_mcfile.size(); i++) {

    if (_mclabel[i].EqualTo(process)) continue;

    _mcfile[i]->cd();

    TH1D* dummy = (TH1D*)_mcfile[i]->Get(level + "/h_counterLum_" + channel);

    float counterDummy = Yield(dummy);

    if (_luminosity_fb > 0 && _mcscale[i] > -999) counterDummy *= _luminosity_fb;

    if (_mcscale[i] > 0) counterDummy *= _mcscale[i];

    counterBackground += counterDummy;
  }


  // Get the data
  //----------------------------------------------------------------------------
  if (_datafile)
    {
      _datafile->cd();

      TH1D* dummy = (TH1D*)_datafile->Get(level + "/h_counterLum_" + channel);

      _datahist = (TH1D*)dummy->Clone();      
    }

  float counterData = Yield(_datahist);


  // Cross-section calculation
  //----------------------------------------------------------------------------  
  float xs = (counterData - counterBackground) / (1e3 * _luminosity_fb * efficiency * branchingratio);
  float mu = (counterData - counterBackground) / (counterSignal);


  // Statistical error
  //----------------------------------------------------------------------------  
  float xsErrorStat = sqrt(counterData) / (1e3 * _luminosity_fb * efficiency * branchingratio);
  float muErrorStat = sqrt(counterData) / (counterSignal); 

 
  // Print
  //----------------------------------------------------------------------------  
  printf("      channel = %s\n", channel.Data());
  printf("        ndata = %.0f\n", counterData);
  printf("         nbkg = %.2f\n", counterBackground);
  printf(" ndata - nbkg = %.2f\n", counterData - counterBackground);
  printf("      nsignal = %.2f\n", counterSignal);
  printf("           mu = (ndata - nbkg) / nsignal = %.2f +- %.2f (stat) +- %.2f (lumi)\n", mu, muErrorStat, mu * lumi_error_percent / 1e2);
  printf("         lumi = %.0f pb\n", 1e3 * _luminosity_fb);
  printf("           br = %f\n", branchingratio);
  printf("          eff = %.4f\n", efficiency);
  printf("           xs = (ndata - nbkg) / (lumi * eff * br) = %.2f +- %.2f (stat) +- %.2f (lumi) pb\n\n", xs, xsErrorStat, xs * lumi_error_percent / 1e2);
}


//-----------------------------------------------------------------------------
// DrawLatex 
//------------------------------------------------------------------------------
void HistogramReader::DrawLatex(Font_t      tfont,
				Float_t     x,
				Float_t     y,
				Float_t     tsize,
				Short_t     align,
				const char* text,
				Bool_t      setndc)
{
  TLatex* tl = new TLatex(x, y, text);

  tl->SetNDC      (setndc);
  tl->SetTextAlign( align);
  tl->SetTextFont ( tfont);
  tl->SetTextSize ( tsize);

  tl->Draw("same");
}


//------------------------------------------------------------------------------
// DrawLegend
//------------------------------------------------------------------------------
TLegend* HistogramReader::DrawLegend(Float_t x1,
				     Float_t y1,
				     TH1*    hist,
				     TString label,
				     TString option,
				     Float_t tsize,
				     Float_t xoffset,
				     Float_t yoffset)
{
  TLegend* legend = new TLegend(x1,
				y1,
				x1 + xoffset,
				y1 + yoffset);
  
  legend->SetBorderSize(    0);
  legend->SetFillColor (    0);
  legend->SetTextAlign (   12);
  legend->SetTextFont  (   42);
  legend->SetTextSize  (tsize);

  TString final_label = Form(" %s", label.Data());

  if (_drawyield && !_publicstyle)
    final_label = Form("%s (%.0f)", final_label.Data(), Yield(hist));

  if (Yield(hist) < 0)
    printf("\n [HistogramReader::DrawLegend] Warning: %s %s yield = %f\n\n",
	   label.Data(),
	   hist->GetName(),
	   Yield(hist));

  legend->AddEntry(hist, final_label.Data(), option.Data());
  legend->Draw();

  WriteYields(hist, label);

  return legend;
}


//------------------------------------------------------------------------------
// GetMaximum
//------------------------------------------------------------------------------
Float_t HistogramReader::GetMaximum(TH1*    hist,
				    Float_t xmin,
				    Float_t xmax,
				    Bool_t  binError)
{
  UInt_t nbins = hist->GetNbinsX();

  TAxis* axis = (TAxis*)hist->GetXaxis();
  
  Int_t firstBin = (xmin != -999) ? axis->FindBin(xmin) : 1;
  Int_t lastBin  = (xmax != -999) ? axis->FindBin(xmax) : nbins;

  Float_t hmax = 0;

  for (Int_t i=firstBin; i<=lastBin; i++) {

    Float_t binHeight = hist->GetBinContent(i);

    if (binError) binHeight += hist->GetBinError(i);

    if (binHeight > hmax) hmax = binHeight;
  }

  return hmax;
}


//------------------------------------------------------------------------------
// MoveOverflows
//
// For all histogram types: nbins, xlow, xup
//
//   bin = 0;       underflow bin
//   bin = 1;       first bin with low-edge xlow INCLUDED
//   bin = nbins;   last bin with upper-edge xup EXCLUDED
//   bin = nbins+1; overflow bin
//
//------------------------------------------------------------------------------
void HistogramReader::MoveOverflows(TH1*    hist,
				    Float_t xmin,
				    Float_t xmax)
{
  int nentries = hist->GetEntries();
  int nbins    = hist->GetNbinsX();
  
  TAxis* xaxis = (TAxis*)hist->GetXaxis();


  // Underflow
  //----------------------------------------------------------------------------
  if (xmin != -999)
    {
      Int_t   firstBin = -1;
      Float_t firstVal = 0;
      Float_t firstErr = 0;
      
      for (Int_t i=0; i<=nbins+1; i++)
	{
	  if (xaxis->GetBinLowEdge(i) < xmin)
	    {
	      firstVal += hist->GetBinContent(i);
	      firstErr += (hist->GetBinError(i)*hist->GetBinError(i));
	      hist->SetBinContent(i, 0);
	      hist->SetBinError  (i, 0);
	    }
	  else if (firstBin == -1)
	    {
	      firstVal += hist->GetBinContent(i);
	      firstErr += (hist->GetBinError(i)*hist->GetBinError(i));
	      firstBin = i;
	    }
	}

      firstErr = sqrt(firstErr);
  
      hist->SetBinContent(firstBin, firstVal);
      hist->SetBinError  (firstBin, firstErr);
    }


  // Overflow
  //----------------------------------------------------------------------------
  if (xmax != -999)
    {
      Int_t   lastBin = -1;
      Float_t lastVal = 0;
      Float_t lastErr = 0;
      
      for (Int_t i=nbins+1; i>=0; i--)
	{
	  Float_t lowEdge = xaxis->GetBinLowEdge(i);
      
	  if (lowEdge >= xmax)
	    {
	      lastVal += hist->GetBinContent(i);
	      lastErr += (hist->GetBinError(i)*hist->GetBinError(i));
	      hist->SetBinContent(i, 0);
	      hist->SetBinError  (i, 0);
	    }
	  else if (lastBin == -1)
	    {
	      lastVal += hist->GetBinContent(i);
	      lastErr += (hist->GetBinError(i)*hist->GetBinError(i));
	      lastBin = i;
	    }
	}

      lastErr = sqrt(lastErr);
  
      hist->SetBinContent(lastBin, lastVal);
      hist->SetBinError  (lastBin, lastErr);
    }

  hist->SetEntries(nentries);
}


//------------------------------------------------------------------------------
// SetAxis
//------------------------------------------------------------------------------
void HistogramReader::SetAxis(TH1*    hist,
			      TString xtitle,
			      TString ytitle,
			      Float_t xoffset,
			      Float_t yoffset)
{
  gPad->cd();
  gPad->Update();

  // See https://root.cern.ch/doc/master/classTAttText.html#T4
  Float_t padw = gPad->XtoPixel(gPad->GetX2());
  Float_t padh = gPad->YtoPixel(gPad->GetY1());

  Float_t size = (padw < padh) ? padw : padh;

  size = 20. / size;  // Like this label size is always 20 pixels
  
  TAxis* xaxis = (TAxis*)hist->GetXaxis();
  TAxis* yaxis = (TAxis*)hist->GetYaxis();

  xaxis->SetTitleOffset(xoffset);
  yaxis->SetTitleOffset(yoffset);

  xaxis->SetLabelSize(size);
  yaxis->SetLabelSize(size);
  xaxis->SetTitleSize(size);
  yaxis->SetTitleSize(size);

  xaxis->SetTitle(xtitle);
  yaxis->SetTitle(ytitle);

  yaxis->CenterTitle();

  gPad->GetFrame()->DrawClone();
  gPad->RedrawAxis();
}


//------------------------------------------------------------------------------
// SetHistogram
//------------------------------------------------------------------------------
void HistogramReader::SetHistogram(TH1*     hist,
				   Color_t  color,
				   Style_t  fstyle,
				   Style_t  mstyle,
				   Style_t  lstyle,
				   Width_t  lwidth,
				   Int_t    ngroup,
				   Bool_t   moveoverflow,
				   Float_t& xmin,
				   Float_t& xmax)
{
  if (!hist)
    {
      printf("\n [HistogramReader::SetHistogram] Error: histogram does not exist\n\n");
      return;
    }

  if (xmin == -999) xmin = hist->GetXaxis()->GetXmin();
  if (xmax == -999) xmax = hist->GetXaxis()->GetXmax();

  hist->SetFillColor(color );
  hist->SetFillStyle(fstyle);

  hist->SetLineColor(color );
  hist->SetLineStyle(lstyle);
  hist->SetLineWidth(lwidth);

  hist->SetMarkerColor(color );
  hist->SetMarkerStyle(mstyle);

  if (_stackoption.Contains("nostack") && Yield(hist) > 0)
    {
      hist->SetFillStyle(0);
      hist->SetLineWidth(2);

      hist->Scale(1. / Yield(hist));
    }


  // Rebin and move overflow bins
  //----------------------------------------------------------------------------
  if (ngroup > 0) hist->Rebin(ngroup);
  
  if (moveoverflow) MoveOverflows(hist, xmin, xmax);
}


//------------------------------------------------------------------------------
// Yield
//------------------------------------------------------------------------------
Float_t HistogramReader::Yield(TH1* hist)
{
  if (!hist) return 0;

  Int_t nbins = hist->GetNbinsX();

  return hist->Integral(0, nbins+1);
}


//------------------------------------------------------------------------------
// Error
//------------------------------------------------------------------------------
Float_t HistogramReader::Error(TH1* hist)
{
  if (!hist) return 0;

  Float_t hist_error = sqrt(hist->GetSumw2()->GetSum());

  return hist_error;
}


//------------------------------------------------------------------------------
// EventsByCut
//------------------------------------------------------------------------------
void HistogramReader::EventsByCut(TFile*  file,
				  TString analysis,
				  TString hname)
{
  // Check if the evolution histogram already exists
  TH1D* test_hist = (TH1D*)file->Get(analysis + "/" + hname + "_evolution");

  if (test_hist) return;


  // Get the number of bins
  Int_t nbins = 0;
  
  for (Int_t i=0; i<ncut; i++)
    {
      if (!scut[i].Contains(analysis + "/")) continue;

      nbins++;
    }


  // Create and fill the evolution histogram
  file->cd(analysis);

  TH1D* hist = new TH1D(hname + "_evolution", "", nbins, -0.5, nbins-0.5);

  for (Int_t i=0, bin=0; i<ncut; i++)
    {
      if (!scut[i].Contains(analysis + "/")) continue;

      TH1D* dummy = (TH1D*)file->Get(scut[i] + "/" + hname);

      bin++;

      hist->SetBinContent(bin, Yield(dummy));
      hist->SetBinError  (bin, Error(dummy));


      // Change the evolution histogram x-axis labels
      TString tok, icut;

      Ssiz_t from = 0;

      while (scut[i].Tokenize(tok, from, "_")) icut = tok;

      hist->GetXaxis()->SetBinLabel(bin, icut);
    }


  // Write the evolution histogram
  hist->Write();
  file->cd();
}


//------------------------------------------------------------------------------
// LoopEventsByCut
//------------------------------------------------------------------------------
void HistogramReader::LoopEventsByCut(TString analysis, TString hname)
{
  if (_datafile) EventsByCut(_datafile, analysis, hname);

  for (UInt_t i=0; i<_mcfile.size(); i++) EventsByCut(_mcfile[i], analysis, hname);

  for (UInt_t i=0; i<_signalfile.size(); i++) EventsByCut(_signalfile[i], analysis, hname);
}


//------------------------------------------------------------------------------
// EventsByChannel
//------------------------------------------------------------------------------
void HistogramReader::EventsByChannel(TFile*  file,
				      TString level)
{
  // Check if the evolution histogram already exists
  TH1D* test_hist = (TH1D*)file->Get(level + "/h_counterLum_evolution");

  if (test_hist) return;


  // Get the number of bins
  Int_t firstchannel = (level.Contains("WZ/")) ? eee : ee;
  Int_t lastchannel  = (level.Contains("WZ/")) ? lll : ll;
  
  Int_t nbins = 0;
  
  for (Int_t i=firstchannel; i<=lastchannel; i++) nbins++;


  // Create and fill the evolution histogram
  file->cd(level);

  TH1D* hist = new TH1D("h_counterLum_evolution", "", nbins, -0.5, nbins-0.5);

  for (Int_t i=firstchannel, bin=0; i<=lastchannel; i++)
    {
      TH1D* dummy = (TH1D*)file->Get(level + "/h_counterLum_" + schannel[i]);

      bin++;

      hist->SetBinContent(bin, Yield(dummy));
      hist->SetBinError  (bin, Error(dummy));

      hist->GetXaxis()->SetBinLabel(bin, lchannel[i]);
    }


  // Write the evolution histogram
  hist->Write();
  file->cd();
}


//------------------------------------------------------------------------------
// LoopEventsByChannel
//------------------------------------------------------------------------------
void HistogramReader::LoopEventsByChannel(TString level)
{
  if (_datafile) EventsByChannel(_datafile, level);

  for (UInt_t i=0; i<_mcfile.size(); i++) EventsByChannel(_mcfile[i], level);

  for (UInt_t i=0; i<_signalfile.size(); i++) EventsByChannel(_signalfile[i], level);
}


//------------------------------------------------------------------------------
// GetBestScoreX
//------------------------------------------------------------------------------
Float_t HistogramReader::GetBestScoreX(TH1*    sig_hist,
				       TH1*    bkg_hist,
				       TString fom)
{
  Int_t nbins = sig_hist->GetNbinsX();

  Float_t score_value = 0;
  Float_t score_x     = 0;
  Float_t sig_total   = Yield(sig_hist);


  // For The Punzi Effect
  // http://arxiv.org/pdf/physics/0308063v2.pdf
  Float_t a = 5.;
  Float_t b = 1.645;  // Corresponds to a p-value equal to 0.05


  for (UInt_t k=0; k<nbins+1; k++) {

    Float_t sig_yield = sig_hist->Integral(k, nbins+1);
    Float_t bkg_yield = bkg_hist->Integral(k, nbins+1);

    Float_t sig_eff = (sig_total > 0.) ? sig_yield / sig_total : -999;

    if (sig_yield > 0. && bkg_yield > 0.)
      {
	Float_t score = -999;

	if (fom.EqualTo("S/sqrt(B)"))   score = sig_yield / sqrt(bkg_yield);
	if (fom.EqualTo("S/sqrt(S+B)")) score = sig_yield / sqrt(sig_yield + bkg_yield);
	if (fom.EqualTo("S/B"))         score = sig_yield / bkg_yield; 
	if (fom.EqualTo("PunziEq6"))    score =   sig_eff / (b*b + 2*a*sqrt(bkg_yield) + b*sqrt(b*b + 4*a*sqrt(b) + 4*bkg_yield)); 
	if (fom.EqualTo("PunziEq7"))    score =   sig_eff / (a/2 + sqrt(bkg_yield));

	if (score > score_value)
	  {
	    score_value = score;
	    score_x     = sig_hist->GetBinCenter(k);
	  }
      }
  }


  printf("\n [HistogramReader::GetBestScoreX] x = %.2f (%.2f < x < %.2f) has the best %s (%f)\n\n",
  	 score_x,
  	 sig_hist->GetXaxis()->GetXmin(),
  	 sig_hist->GetXaxis()->GetXmax(),
   	 fom.Data(),
  	 score_value);


  return score_x;
}


//------------------------------------------------------------------------------
// GetBestSignalScoreX
//------------------------------------------------------------------------------
Float_t HistogramReader::GetBestSignalScoreX(TString hname,
					     TString fom,
					     Int_t   ngroup)
{
  printf("\n [HistogramReader::GetBestSignalScoreX] Warning: reading only the first signal\n");


  // Get the signals
  //----------------------------------------------------------------------------
  _signalhist.clear();

  for (UInt_t i=0; i<_signalfile.size(); i++) {

    _signalfile[i]->cd();

    TH1D* dummy = (TH1D*)_signalfile[i]->Get(hname);

    _signalhist.push_back((TH1D*)dummy->Clone());

    if (ngroup > 0) _signalhist[i]->Rebin(ngroup);
  }

  
  // Get the backgrounds
  //----------------------------------------------------------------------------
  _mchist.clear();

  THStack* mcstack = new THStack(hname + "_mcstack", hname + "_mcstack");

  for (UInt_t i=0; i<_mcfile.size(); i++) {

    _mcfile[i]->cd();

    TH1D* dummy = (TH1D*)_mcfile[i]->Get(hname);

    _mchist.push_back((TH1D*)dummy->Clone());

    if (ngroup > 0) _mchist[i]->Rebin(ngroup);

    mcstack->Add(_mchist[i]);
  }


  // Get the best score
  //----------------------------------------------------------------------------
  TH1D* backgroundhist = (TH1D*)(mcstack->GetStack()->Last());

  return GetBestScoreX(_signalhist[0], backgroundhist, fom);
}


//------------------------------------------------------------------------------
// WriteYields
//------------------------------------------------------------------------------
void HistogramReader::WriteYields(TH1*    hist,
				  TString label)
{
  TString hname = hist->GetName();

  if (!_writeyields) return;

  if (_writelabels)
    {
      _writelabels = false;

      _yields_table << Form("\n %14s", " ");
        
      for (int i=1; i<=hist->GetNbinsX(); i++) {

	TString binlabel = (TString)hist->GetXaxis()->GetBinLabel(i);
	    
	_yields_table << Form(" | %-32s", binlabel.Data());
      }

      _yields_table << Form("\n");
    }

  _yields_table << Form(" %14s", label.Data());

  for (int i=1; i<=hist->GetNbinsX(); i++) {

    float process_yield = hist->GetBinContent(i);
    float process_error = sqrt(hist->GetSumw2()->At(i));

    if (label.EqualTo("data"))
      {
	_yields_table << Form(" | %8.0f %14s", process_yield, " ");
      }
    else
      {
	_yields_table << Form(" | %11.2f +/- %7.2f", process_yield, process_error);
      }

    int denominator = (hname.Contains("counterLum_evolution")) ? hist->GetNbinsX() : 1;

    float process_percent = 1e2 * process_yield / hist->GetBinContent(denominator);

    _yields_table << Form(" (%5.1f%s)", process_percent, "%");
  }

  _yields_table << Form("\n");
}


//------------------------------------------------------------------------------
// Roc
//------------------------------------------------------------------------------
void HistogramReader::Roc(TString hname,
			  TString xtitle,
			  Int_t   npoints,
			  TString units,
			  Float_t xmin,
			  Float_t xmax)
{
  // Get the signal
  //----------------------------------------------------------------------------
  THStack* stack_sig = new THStack(hname + "_stack_sig", hname + "_stack_sig");

  TFile* file_sig[_roc_signals.size()];

  for (int i=0; i<_roc_signals.size(); ++i)
    {
      file_sig[i] = new TFile(_roc_signals.at(i));
      
      stack_sig->Add((TH1D*)file_sig[i]->Get(hname));
    }

  TH1D* hSig = (TH1D*)(stack_sig->GetStack()->Last());


  // Get the backgrounds
  //----------------------------------------------------------------------------
  THStack* stack_bkg = new THStack(hname + "_stack_bkg", hname + "_stack_bkg");

  TFile* file_bkg[_roc_backgrounds.size()];

  for (int j=0; j<_roc_backgrounds.size(); ++j)
    {
      file_bkg[j] = new TFile(_roc_backgrounds.at(j));

      stack_bkg->Add((TH1D*)file_bkg[j]->Get(hname));
    }

  TH1D* hBkg = (TH1D*)(stack_bkg->GetStack()->Last());


  // Compute ROC and significance
  //----------------------------------------------------------------------------
  float step = (xmax - xmin) / npoints;

  TGraph* rocGraph = new TGraph();
  TGraph* sigGraph = new TGraph();

  Float_t score_value = 0;
  Float_t score_x     = 0;

  Float_t sigTotal = hSig->Integral(-1, -1);
  Float_t bkgTotal = hBkg->Integral(-1, -1);

  for (int s=0; s<=npoints; ++s) {

    Float_t sigYield = 0;
    Float_t bkgYield = 0;

    sigYield += hSig->Integral(-1, hSig->FindBin(xmin + s*step));
    bkgYield += hBkg->Integral(-1, hBkg->FindBin(xmin + s*step));

    Float_t sigEff = (sigTotal != 0) ? sigYield / sigTotal : -999;
    Float_t bkgEff = (bkgTotal != 0) ? bkgYield / bkgTotal : -999;

    Float_t score = sigYield / sqrt(sigYield + bkgYield);

    if (score > score_value) {
      score_value = score;
      score_x     = xmin + s*step;
    }

    rocGraph->SetPoint(s, sigEff, 1 - bkgEff);
    sigGraph->SetPoint(s, xmin + s*step, score);
  }


  printf("\n [HistogramReader::Roc] Reading %s\n", hname.Data());
  printf(" The best S/sqrt(S+B) = %f corresponds to x = %.2f %s (%.2f < x < %.2f)\n\n",
	 score_value, score_x, units.Data(), xmin, xmax);
  

  // Draw and save ROC
  //----------------------------------------------------------------------------
  TCanvas* rocCanvas = new TCanvas(hname + " ROC", hname + " ROC");

  rocGraph->SetMarkerColor(kRed+1);
  rocGraph->SetMarkerStyle(kFullCircle);
  rocGraph->SetMarkerSize(0.5);

  rocGraph->Draw("ap");

  rocGraph->GetXaxis()->SetRangeUser(0, 1);
  rocGraph->GetYaxis()->SetRangeUser(0, 1);

  DrawLatex(42, 0.190, 0.945, 0.050, 11, _title);

  SetAxis(rocGraph->GetHistogram(), xtitle + " signal efficiency", xtitle + " background rejection", 1.5, 1.8);

  if (_savepdf) rocCanvas->SaveAs(_outputdir + hname + "_ROC.pdf");
  if (_savepng) rocCanvas->SaveAs(_outputdir + hname + "_ROC.png");


  // Draw and save significance
  //----------------------------------------------------------------------------
  TCanvas *sigCanvas = new TCanvas(hname + " significance", hname + " significance");

  TString myxtitle = (!units.Contains("NULL")) ? xtitle + " [" + units + "]" : xtitle;

  sigGraph->SetMarkerColor(kRed+1);
  sigGraph->SetMarkerStyle(kFullCircle);
  sigGraph->SetMarkerSize(0.5);

  sigGraph->Draw("ap");

  sigGraph->GetXaxis()->SetRangeUser(xmin, xmax);
  sigGraph->GetYaxis()->SetRangeUser(0, 1.5*score_value);

  DrawLatex(42, 0.190, 0.945, 0.050, 11, _title);

  SetAxis(sigGraph->GetHistogram(), myxtitle, "S / #sqrt{S + B}", 1.5, 2.1);

  if (_savepdf) sigCanvas->SaveAs(_outputdir + hname + "_significance.pdf");
  if (_savepng) sigCanvas->SaveAs(_outputdir + hname + "_significance.png");
}
