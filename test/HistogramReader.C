#include "HistogramReader.h"


//------------------------------------------------------------------------------
// HistogramReader
//------------------------------------------------------------------------------
HistogramReader::HistogramReader(TString const &inputdir,
				 TString const &outputdir) :
  _inputdir     (inputdir),
  _outputdir    (outputdir),
  _luminosity_fb(0),
  _drawratio    (true),
  _savepdf      (true),
  _savepng      (true)
{
  _mcfile.clear();
  _mccolor.clear();
  _mclabel.clear();
}


//------------------------------------------------------------------------------
// AddProcess
//------------------------------------------------------------------------------
void HistogramReader::AddProcess(TString const &filename,
				 TString const &label,
				 Color_t        color)
{
  TFile *file = new TFile(_inputdir + filename + ".root");

  if (filename.Contains("Data"))
    {
      _datafile  = file;
      _datalabel = label;
      _datacolor = color;
    }
  else
    {
      _mcfile.push_back(file);
      _mclabel.push_back(label);
      _mccolor.push_back(color);
    }
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

  if (setlogy) cname += "_log";

  TCanvas* canvas = NULL;

  TPad* pad1 = NULL;
  TPad* pad2 = NULL;

  if (_drawratio)
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
      canvas = new TCanvas(cname, cname);

      pad1 = new TPad("pad1", "pad1", 0, 0, 1, 1);
      pad1->Draw();
    }


  //----------------------------------------------------------------------------
  // pad1
  //----------------------------------------------------------------------------
  pad1->cd();
  
  pad1->SetLogy(setlogy);

  TH1D* dummy = (TH1D*)_datafile->Get(hname);

  if (!dummy)
    {
      printf("\n [HistogramReader::Draw] %s not found\n\n", hname.Data());
      return;
    }

  _datahist = (TH1D*)dummy->Clone();

  _datahist->SetFillColor(_datacolor);
  _datahist->SetLineColor(_datacolor);
  _datahist->SetMarkerStyle(kFullCircle);
  _datahist->SetTitle("");

  if (xmin == -999) xmin = _datahist->GetXaxis()->GetXmin();
  if (xmax == -999) xmax = _datahist->GetXaxis()->GetXmax();

  if (ngroup > 0) _datahist->Rebin(ngroup);
  
  if (moveoverflow) MoveOverflows(_datahist, xmin, xmax);

  _mchist.clear();

  THStack* hstack = new THStack(hname, hname);

  for (UInt_t i=0; i<_mcfile.size(); i++) {

    dummy = (TH1D*)_mcfile[i]->Get(hname);

    _mchist.push_back((TH1D*)dummy->Clone());

    if (ngroup > 0) _mchist[i]->Rebin(ngroup);

    if (moveoverflow) MoveOverflows(_mchist[i], xmin, xmax);

    _mchist[i]->SetFillColor(_mccolor[i]);
    _mchist[i]->SetLineColor(_mccolor[i]);
    _mchist[i]->SetLineWidth(0);
    _mchist[i]->SetFillStyle(1001);

    hstack->Add(_mchist[i]);
  }


  // All MC
  //----------------------------------------------------------------------------
  TH1D* allmc = (TH1D*)_datahist->Clone("allmc");

  allmc->SetFillColor  (kGray+1);
  allmc->SetFillStyle  (   3345);
  allmc->SetLineColor  (kGray+1);
  allmc->SetMarkerColor(kGray+1);
  allmc->SetMarkerSize (      0);

  for (Int_t ibin=0; ibin<=allmc->GetNbinsX()+1; ibin++) {

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

    allmc->SetBinContent(ibin, binValue);
    allmc->SetBinError  (ibin, binError);
  }


  // Draw
  //----------------------------------------------------------------------------
  _datahist->Draw("ep");
  hstack   ->Draw("hist,same");
  allmc    ->Draw("e2,same");
  _datahist->Draw("ep,same");


  // Set xtitle and ytitle
  //----------------------------------------------------------------------------
  TString ytitle = Form("events / %s.%df", "%", precision);

  ytitle = Form(ytitle.Data(), _datahist->GetBinWidth(0));

  if (!units.Contains("NULL")) {
    xtitle = Form("%s [%s]", xtitle.Data(), units.Data());
    ytitle = Form("%s %s",   ytitle.Data(), units.Data());
  }


  // Adjust xaxis and yaxis
  //----------------------------------------------------------------------------
  _datahist->GetXaxis()->SetRangeUser(xmin, xmax);

  Float_t theMin   = 0.0;
  Float_t theMax   = GetMaximum(_datahist, xmin, xmax);
  Float_t theMaxMC = GetMaximum(allmc,      xmin, xmax);

  if (theMaxMC > theMax) theMax = theMaxMC;

  if (pad1->GetLogy())
    {
      theMin = 1e-1;
      theMax = 2. * TMath::Power(10, TMath::Log10(theMax) + 2);
    }
  else
    {
      theMax *= 1.45;
    }

  _datahist->SetMinimum(theMin);
  _datahist->SetMaximum(theMax);

  if (ymin != -999) _datahist->SetMinimum(ymin);
  if (ymax != -999) _datahist->SetMaximum(ymax);


  // Legend
  //----------------------------------------------------------------------------
  Float_t x0     = 0.222;
  Float_t y0     = 0.834;
  Float_t xdelta = 0.0;
  Float_t ydelta = _yoffset + 0.001;
  Int_t   ny     = 0;

  DrawLegend(x0 + xdelta, y0 - ny*ydelta, _datahist, Form(" %s (%.0f)", _datalabel.Data(), Yield(_datahist)), "lp"); ny++;
  DrawLegend(x0 + xdelta, y0 - ny*ydelta, allmc,     Form(" all (%.0f)",                   Yield(allmc)),     "f");  ny++;

  for (int i=0; i<_mchist.size(); i++)
    {
      if (ny == 3)
	{
	  ny = 0;
	  xdelta += 0.228;
	}

      DrawLegend(x0 + xdelta, y0 - ny*ydelta, _mchist[i], Form(" %s (%.0f)", _mclabel[i].Data(), Yield(_mchist[i])), "f");
      ny++;
    }


  // Titles
  //----------------------------------------------------------------------------
  Float_t xprelim = (_drawratio) ? 0.288 : 0.300;

  DrawTLatex(61, 0.190,   0.945, 0.050, 11, "CMS");
  DrawTLatex(52, xprelim, 0.945, 0.030, 11, "Preliminary");
  DrawTLatex(42, 0.940,   0.945, 0.050, 31, Form("%.3f fb^{-1} (13TeV)", _luminosity_fb));

  SetAxis(_datahist, xtitle, ytitle, 0.045, 1.5, 1.7);


  //----------------------------------------------------------------------------
  // pad2
  //----------------------------------------------------------------------------
  if (_drawratio)
    {
      pad2->cd();
    
      TH1D* ratio       = (TH1D*)_datahist->Clone("ratio");
      TH1D* uncertainty = (TH1D*)allmc->Clone("uncertainty");

      for (Int_t ibin=1; ibin<=ratio->GetNbinsX(); ibin++) {

	Float_t mcValue = allmc->GetBinContent(ibin);
	Float_t mcError = allmc->GetBinError  (ibin);
    
	Float_t dtValue = ratio->GetBinContent(ibin);
	Float_t dtError = ratio->GetBinError  (ibin);

	Float_t ratioVal         = 0.0;
	Float_t ratioErr         = 1e-9;
	Float_t uncertaintyError = 1e-9;

	if (mcValue > 0 && dtValue > 0)
	  {
	    ratioVal         = dtValue / mcValue - 1.0;
	    ratioErr         = dtError / mcValue;
	    uncertaintyError = ratioVal * mcError / mcValue;
	  }
	
	ratio->SetBinContent(ibin, ratioVal);
	ratio->SetBinError  (ibin, ratioErr);
	
	uncertainty->SetBinContent(ibin, 0.0);
	uncertainty->SetBinError  (ibin, uncertaintyError);
      }

      ratio->Draw("ep");

      ratio->GetYaxis()->SetRangeUser(-2, 2);

      uncertainty->Draw("e2,same");

      ratio->Draw("ep,same");

      SetAxis(ratio, xtitle, "data / MC - 1", 0.105, 1.4, 0.75);
    }


  //----------------------------------------------------------------------------
  // Save it
  //----------------------------------------------------------------------------
  canvas->cd();

  if (_savepdf) canvas->SaveAs(_outputdir + cname + ".pdf");
  if (_savepng) canvas->SaveAs(_outputdir + cname + ".png");
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

  legend->AddEntry(hist, label.Data(), option.Data());
  legend->Draw();

  return legend;
}


//------------------------------------------------------------------------------
// DrawTLatex
//------------------------------------------------------------------------------
void HistogramReader::DrawTLatex(Font_t      tfont,
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
// GetMaximum
//------------------------------------------------------------------------------
Float_t HistogramReader::GetMaximum(TH1*     hist,
				     Float_t xmin,
				     Float_t xmax)
{
  UInt_t nbins = hist->GetNbinsX();

  TAxis* axis = (TAxis*)hist->GetXaxis();
  
  Int_t firstBin = (xmin != -999) ? axis->FindBin(xmin) : 1;
  Int_t lastBin  = (xmax != -999) ? axis->FindBin(xmax) : nbins;

  Float_t maxWithErrors = 0;

  for (Int_t i=firstBin; i<=lastBin; i++) {

    Float_t binHeight = hist->GetBinContent(i) + hist->GetBinError(i);

    if (binHeight > maxWithErrors) maxWithErrors = binHeight;
  }

  return maxWithErrors;
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

  int nbins = hist->GetNbinsX();
  
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
			      Float_t size,
			      Float_t xoffset,
			      Float_t yoffset)
{
  gPad->cd();
  gPad->Update();

  TAxis* xaxis = (TAxis*)hist->GetXaxis();
  TAxis* yaxis = (TAxis*)hist->GetYaxis();

  xaxis->SetLabelFont(42);
  yaxis->SetLabelFont(42);
  xaxis->SetTitleFont(42);
  yaxis->SetTitleFont(42);

  xaxis->SetLabelOffset(0.025);
  yaxis->SetLabelOffset(0.025);
  xaxis->SetTitleOffset(xoffset);
  yaxis->SetTitleOffset(yoffset);

  xaxis->SetLabelSize(size);
  yaxis->SetLabelSize(size);
  xaxis->SetTitleSize(size);
  yaxis->SetTitleSize(size);

  xaxis->SetTitle(xtitle);
  yaxis->SetTitle(ytitle);

  xaxis->SetNdivisions(505);
  yaxis->SetNdivisions(505);

  yaxis->CenterTitle();

  gPad->GetFrame()->DrawClone();
  gPad->RedrawAxis();
}


//------------------------------------------------------------------------------
// Yield
//------------------------------------------------------------------------------
Float_t HistogramReader::Yield(TH1* hist)
{
  if (hist)
    {
      Int_t nbins = hist->GetNbinsX();
      
      return hist->Integral(0, nbins+1);
    }
  else
    {
      return 0.;
    }
}