// Tests for RooNLLVar and the other test statistics
// Authors: Stephan Hageboeck, CERN 10/2020
//          Jonas Rembser, CERN 10/2022

#include <RooAddPdf.h>
#include <RooBinning.h>
#include <RooDataHist.h>
#include <RooDataSet.h>
#include <RooFitResult.h>
#include <RooHelpers.h>
#include <RooHistPdf.h>
#include <RooNLLVar.h>
#include <RooRandom.h>
#include <RooPlot.h>
#include <RooRealVar.h>
#include <RooWorkspace.h>

#include <gtest/gtest.h>

// Backward compatibility for gtest version < 1.10.0
#ifndef INSTANTIATE_TEST_SUITE_P
#define INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

#include <memory>
#include <cmath>

namespace {

double getVal(const char *name, const RooArgSet &set)
{
   return static_cast<const RooRealVar &>(set[name]).getVal();
}

double getErr(const char *name, const RooArgSet &set)
{
   return static_cast<const RooRealVar &>(set[name]).getError();
}

} // namespace

class TestStatisticTest : public testing::TestWithParam<std::tuple<std::string>> {
   void SetUp() override
   {
      _batchMode = std::get<0>(GetParam());
      _changeMsgLvl = std::make_unique<RooHelpers::LocalChangeMsgLevel>(RooFit::ERROR);
   }

   void TearDown() override { _changeMsgLvl.reset(); }

protected:
   std::string _batchMode;

private:
   std::unique_ptr<RooHelpers::LocalChangeMsgLevel> _changeMsgLvl;
};

TEST_P(TestStatisticTest, IntegrateBins)
{
   RooRandom::randomGenerator()->SetSeed(1337ul);

   RooWorkspace ws;
   ws.factory("Power::pow(x[0.1, 5.1], {1.0}, {a[-0.3, -5., 5.]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &a = *ws.var("a");
   RooAbsPdf &pdf = *ws.pdf("pow");

   x.setBins(10);

   RooArgSet targetValues;
   RooArgSet(a).snapshot(targetValues);

   using namespace RooFit;

   std::unique_ptr<RooDataHist> dataH(pdf.generateBinned(x, 10000));
   RooRealVar w("w", "weight", 0., 0., 10000.);
   RooDataSet data("data", "data", {x, w}, WeightVar(w));
   for (int i = 0; i < dataH->numEntries(); ++i) {
      auto coords = dataH->get(i);
      data.add(*coords, dataH->weight());
   }

   std::unique_ptr<RooPlot> frame(x.frame());
   dataH->plotOn(frame.get(), MarkerColor(kRed));
   data.plotOn(frame.get(), Name("data"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit1(pdf.fitTo(data, Save(), PrintLevel(-1), BatchMode(_batchMode)));
   pdf.plotOn(frame.get(), LineColor(kRed), Name("standard"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit2(
      pdf.fitTo(data, Save(), PrintLevel(-1), BatchMode(_batchMode), IntegrateBins(1.E-3)));
   pdf.plotOn(frame.get(), LineColor(kBlue), Name("highRes"));

   EXPECT_GT(std::abs(getVal("a", targetValues) - getVal("a", fit1->floatParsFinal())),
             1. * getErr("a", fit1->floatParsFinal()))
      << "Expecting a bias when sampling PDF in bin centre.";

   EXPECT_NEAR(getVal("a", targetValues), getVal("a", fit2->floatParsFinal()), 1. * getErr("a", fit2->floatParsFinal()))
      << "Expect reduced bias with high-resolution sampling.";

   EXPECT_GT(frame->chiSquare("standard", "data", 1) * 0.9, frame->chiSquare("highRes", "data", 1))
      << "Expect chi2/ndf at least 10% better.";
}

/// Prepare a RooDataSet that looks like the one that HistFactory uses:
/// It pretends to be an unbinned dataset, but instead of single events,
/// events are aggregated in the bin centres using weights.
TEST_P(TestStatisticTest, IntegrateBins_SubRange)
{
   RooRandom::randomGenerator()->SetSeed(1337ul);

   RooWorkspace ws;
   ws.factory("Power::pow(x[0.1, 5.1], {1.0}, {a[-0.3, -5., 5.]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &a = *ws.var("a");
   RooAbsPdf &pdf = *ws.pdf("pow");

   x.setBins(10);
   x.setRange("range", 0.1, 4.1);
   x.setBins(8, "range"); // consistent binning

   RooArgSet targetValues;
   RooArgSet(a).snapshot(targetValues);

   using namespace RooFit;

   std::unique_ptr<RooDataHist> dataH(pdf.generateBinned(x, 10000));
   RooRealVar w("w", "weight", 0., 0., 10000.);
   RooDataSet data("data", "data", {x, w}, WeightVar(w));
   for (int i = 0; i < dataH->numEntries(); ++i) {
      auto coords = dataH->get(i);
      data.add(*coords, dataH->weight());
   }

   std::unique_ptr<RooPlot> frame(x.frame());
   dataH->plotOn(frame.get(), MarkerColor(kRed));
   data.plotOn(frame.get(), Name("data"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit1(
      pdf.fitTo(data, Save(), PrintLevel(-1), Optimize(0), Range("range"), BatchMode(_batchMode)));
   pdf.plotOn(frame.get(), LineColor(kRed), Name("standard"), Range("range"), NormRange("range"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit2(pdf.fitTo(data, Save(), PrintLevel(-1), Optimize(0), Range("range"),
                                                BatchMode(_batchMode), IntegrateBins(1.E-3)));
   pdf.plotOn(frame.get(), LineColor(kBlue), Name("highRes"), Range("range"), NormRange("range"));

   EXPECT_GT(std::abs(getVal("a", targetValues) - getVal("a", fit1->floatParsFinal())),
             1. * getErr("a", fit1->floatParsFinal()))
      << "Expecting a bias when sampling PDF in bin centre.";

   EXPECT_NEAR(getVal("a", targetValues), getVal("a", fit2->floatParsFinal()), 1. * getErr("a", fit2->floatParsFinal()))
      << "Expect reduced bias with high-resolution sampling.";

   EXPECT_GT(frame->chiSquare("standard", "data", 1) * 0.9, frame->chiSquare("highRes", "data", 1))
      << "Expect chi2/ndf at least 10% better.";
}

/// Prepare a RooDataSet that looks like the one that HistFactory uses:
/// It pretends to be an unbinned dataset, but instead of single events,
/// events are aggregated in the bin centres using weights.
TEST_P(TestStatisticTest, IntegrateBins_CustomBinning)
{
   RooRandom::randomGenerator()->SetSeed(1337ul);

   RooWorkspace ws;
   ws.factory("Power::pow(x[1.0, 5.], {1.0}, {a[-0.3, -5., 5.]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &a = *ws.var("a");
   RooAbsPdf &pdf = *ws.pdf("pow");

   RooBinning binning(1., 5.);
   binning.addBoundary(1.5);
   binning.addBoundary(2.0);
   binning.addBoundary(3.);
   binning.addBoundary(4.);
   x.setBinning(binning);

   RooArgSet targetValues;
   RooArgSet(a).snapshot(targetValues);

   using namespace RooFit;

   std::unique_ptr<RooDataHist> dataH(pdf.generateBinned(x, 50000));
   RooRealVar w("w", "weight", 0., 0., 1000000.);
   RooDataSet data("data", "data", {x, w}, WeightVar(w));
   for (int i = 0; i < dataH->numEntries(); ++i) {
      auto coords = dataH->get(i);
      data.add(*coords, dataH->weight());
   }

   std::unique_ptr<RooPlot> frame(x.frame());
   dataH->plotOn(frame.get(), Name("dataHist"), MarkerColor(kRed));
   data.plotOn(frame.get(), Name("data"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit1(pdf.fitTo(data, Save(), PrintLevel(-1), BatchMode(_batchMode), Optimize(0)));
   pdf.plotOn(frame.get(), LineColor(kRed), Name("standard"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit2(
      pdf.fitTo(data, Save(), PrintLevel(-1), Optimize(0), BatchMode(_batchMode), IntegrateBins(1.E-3)));
   pdf.plotOn(frame.get(), LineColor(kBlue), Name("highRes"));

   EXPECT_GT(std::abs(getVal("a", targetValues) - getVal("a", fit1->floatParsFinal())),
             1. * getErr("a", fit1->floatParsFinal()))
      << "Expecting a bias when sampling PDF in bin centre.";

   EXPECT_NEAR(getVal("a", targetValues), getVal("a", fit2->floatParsFinal()), 1. * getErr("a", fit2->floatParsFinal()))
      << "Expect reduced bias with high-resolution sampling.";

   // Note: We cannot compare with the unbinned dataset here, because when it's plotted, it's filled into a
   // histogram with uniform binning. It therefore creates a jumpy distribution. When comparing with the original
   // data hist, we don't get those jumps.
   EXPECT_GT(frame->chiSquare("standard", "dataHist", 1) * 0.9, frame->chiSquare("highRes", "dataHist", 1))
      << "Expect chi2/ndf at least 10% better.";
}

/// Test the same, but now with RooDataHist. Here, the feature should switch on automatically.
TEST_P(TestStatisticTest, IntegrateBins_RooDataHist)
{
   RooWorkspace ws;
   ws.factory("Power::pow(x[0.1, 5.0], {1.0}, {a[-0.3, -5., 5.]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &a = *ws.var("a");
   RooAbsPdf &pdf = *ws.pdf("pow");

   x.setBins(10);

   RooArgSet targetValues;
   RooArgSet(a).snapshot(targetValues);

   using namespace RooFit;

   std::unique_ptr<RooDataHist> data(pdf.generateBinned(x, 10000));

   std::unique_ptr<RooPlot> frame(x.frame());
   data->plotOn(frame.get(), Name("data"));

   a.setVal(3.);
   // Disable IntegrateBins forcefully
   std::unique_ptr<RooFitResult> fit1(
      pdf.fitTo(*data, Save(), PrintLevel(-1), BatchMode(_batchMode), IntegrateBins(-1.)));
   pdf.plotOn(frame.get(), LineColor(kRed), Name("standard"));

   a.setVal(3.);
   // Auto-enable IntegrateBins for all RooDataHists.
   std::unique_ptr<RooFitResult> fit2(
      pdf.fitTo(*data, Save(), PrintLevel(-1), BatchMode(_batchMode), IntegrateBins(0.)));
   pdf.plotOn(frame.get(), LineColor(kBlue), Name("highRes"));

   EXPECT_GT(std::abs(getVal("a", targetValues) - getVal("a", fit1->floatParsFinal())),
             1. * getErr("a", fit1->floatParsFinal()))
      << "Expecting a bias when sampling PDF in bin centre.";

   EXPECT_NEAR(getVal("a", targetValues), getVal("a", fit2->floatParsFinal()), 1. * getErr("a", fit2->floatParsFinal()))
      << "Expect reduced bias with high-resolution sampling.";

   EXPECT_GT(frame->chiSquare("standard", "data", 1) * 0.9, frame->chiSquare("highRes", "data", 1))
      << "Expect chi2/ndf at least 10% better.";
}

TEST(RooChi2Var, IntegrateBins)
{
   RooHelpers::LocalChangeMsgLevel changeMsgLvl(RooFit::WARNING);

   RooRandom::randomGenerator()->SetSeed(1337ul);

   RooWorkspace ws;
   ws.factory("Power::pow(x[0.1, 5.1], {1.0}, {a[-0.3, -5., 5.]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &a = *ws.var("a");
   RooAbsPdf &pdf = *ws.pdf("pow");

   x.setBins(10);

   RooArgSet targetValues;
   RooArgSet(a).snapshot(targetValues);

   using namespace RooFit;

   std::unique_ptr<RooDataHist> dataH(pdf.generateBinned(x, 10000));

   std::unique_ptr<RooPlot> frame(x.frame());
   dataH->plotOn(frame.get(), MarkerColor(kRed));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit1(pdf.chi2FitTo(*dataH, Save(), PrintLevel(-1)));
   pdf.plotOn(frame.get(), LineColor(kRed), Name("standard"));

   a.setVal(3.);
   std::unique_ptr<RooFitResult> fit2(pdf.chi2FitTo(*dataH, Save(), PrintLevel(-1), IntegrateBins(1.E-3)));
   pdf.plotOn(frame.get(), LineColor(kBlue), Name("highRes"));

   EXPECT_GT(std::abs(getVal("a", targetValues) - getVal("a", fit1->floatParsFinal())),
             1. * getErr("a", fit1->floatParsFinal()))
      << "Expecting a bias when sampling PDF in bin centre.";

   EXPECT_NEAR(getVal("a", targetValues), getVal("a", fit2->floatParsFinal()), 1. * getErr("a", fit2->floatParsFinal()))
      << "Expect reduced bias with high-resolution sampling.";

   EXPECT_GT(frame->chiSquare("standard", nullptr, 1) * 0.9, frame->chiSquare("highRes", nullptr, 1))
      << "Expect chi2/ndf at least 10% better.";
}

/// Verifies that a ranged RooNLLVar has still the correct value when copied,
/// as it happens when it is plotted Covers JIRA ticket ROOT-9752.
TEST(RooNLLVar, CopyRangedNLL)
{
   RooHelpers::LocalChangeMsgLevel changeMsgLvl(RooFit::WARNING);

   RooWorkspace ws;
   ws.factory("Gaussian::model(x[0, 10], mean[5, 0, 10], sigma[0.5, 0.01, 5.0])");

   RooRealVar &x = *ws.var("x");
   RooAbsPdf &model = *ws.pdf("model");

   x.setRange("fitrange", 0, 10);

   std::unique_ptr<RooDataSet> ds{model.generate(x, 20)};

   // This bug is related to the implementation details of the old test statistics, so BatchMode is forced to be off
   using namespace RooFit;
   std::unique_ptr<RooNLLVar> nll{static_cast<RooNLLVar *>(model.createNLL(*ds, BatchMode("off")))};
   std::unique_ptr<RooNLLVar> nllrange{
      static_cast<RooNLLVar *>(model.createNLL(*ds, Range("fitrange"), BatchMode("off")))};

   auto nllClone = std::make_unique<RooNLLVar>(*nll);
   auto nllrangeClone = std::make_unique<RooNLLVar>(*nllrange);

   EXPECT_FLOAT_EQ(nll->getVal(), nllClone->getVal());
   EXPECT_FLOAT_EQ(nll->getVal(), nllrange->getVal());
   EXPECT_FLOAT_EQ(nllrange->getVal(), nllrangeClone->getVal());
}

class OffsetBinTest : public testing::TestWithParam<std::tuple<std::string, bool, bool, bool>> {
   void SetUp() override
   {
      _changeMsgLvl = std::make_unique<RooHelpers::LocalChangeMsgLevel>(RooFit::ERROR);
      _batchMode = std::get<0>(GetParam());
      _binned = std::get<1>(GetParam());
      _ext = std::get<2>(GetParam());
      _sumw2 = std::get<3>(GetParam());
   }

   void TearDown() override { _changeMsgLvl.reset(); }

protected:
   std::string _batchMode;
   bool _binned = false;
   bool _ext = false;
   bool _sumw2 = false;

private:
   std::unique_ptr<RooHelpers::LocalChangeMsgLevel> _changeMsgLvl;
};

// Test the `Offset("bin")` feature of RooAbsPdf::createNLL. Doing the
// bin-by-bin offset is equivalent to calculating the likelihood ratio with the
// NLL of a template histogram that is based of the dataset, so we use this
// relation to do a cross check: if we create a template pdf from the fit data
// and fit this template to the data with the `Offset("bin")` option, the
// resulting NLL should always be zero (within some numerical errors).
TEST_P(OffsetBinTest, CrossCheck)
{
   using namespace RooFit;
   using RealPtr = std::unique_ptr<RooAbsReal>;

   // Create extended PDF model
   RooWorkspace ws;
   ws.factory("Gaussian::gauss(x[-10, 10], mean[0, -10, 10], sigma[4, 0.1, 10])");
   ws.factory("AddPdf::extGauss({gauss}, {nEvents[10000, 100, 100000]})");

   RooRealVar &x = *ws.var("x");
   RooRealVar &nEvents = *ws.var("nEvents");
   RooAbsPdf &extGauss = *ws.pdf("extGauss");

   // We have to generate double the number of events because in the next step
   // we will weight down each event by a factor of two.
   std::unique_ptr<RooDataSet> data{extGauss.generate(x, 2. * nEvents.getVal())};

   // Replace dataset with a clone where the weights are different from unity
   // such that the effect of the SumW2Error option is not trivial and we test
   // it correctly.
   {
      // Create weighted dataset and hist to test SumW2 feature
      RooRealVar weight("weight", "weight", 0.5, 0.0, 1.0);
      auto dataW = std::make_unique<RooDataSet>("dataW", "dataW", RooArgSet{x, weight}, "weight");
      for (int i = 0; i < data->numEntries(); ++i) {
         dataW->add(*data->get(i), 0.5);
      }
      std::swap(dataW, data);
   }

   std::unique_ptr<RooDataHist> hist{data->binnedClone()};

   // Create template PDF based on data
   RooHistPdf histPdf{"histPdf", "histPdf", x, *hist};
   RooAddPdf extHistPdf("extHistPdf", "extHistPdf", histPdf, nEvents);

   RooAbsData *fitData = _binned ? static_cast<RooAbsData *>(hist.get()) : static_cast<RooAbsData *>(data.get());

   RealPtr nll0{extHistPdf.createNLL(*fitData, BatchMode(_batchMode), Extended(_ext))};
   RealPtr nll1{extHistPdf.createNLL(*fitData, Offset("bin"), BatchMode(_batchMode), Extended(_ext))};

   if (_sumw2) {
      nll0->applyWeightSquared(true);
      nll1->applyWeightSquared(true);
   }

   double nllVal0 = nll0->getVal();
   double nllVal1 = nll1->getVal();

   // For all configurations, the bin offset should have the effect of bringing
   // the NLL to zero, modulo some numerical imprecisions:
   EXPECT_NEAR(nllVal1, 0.0, 1e-8) << "NLL with bin offsetting is " << nllVal1 << ", and " << nllVal0 << " without it.";
}

INSTANTIATE_TEST_SUITE_P(RooNLLVar, TestStatisticTest, testing::Combine(testing::Values("Off", "Cpu")),
                         [](testing::TestParamInfo<TestStatisticTest::ParamType> const &paramInfo) {
                            std::stringstream ss;
                            ss << "BatchMode" << std::get<0>(paramInfo.param);
                            ss << std::get<0>(paramInfo.param);
                            return ss.str();
                         });

INSTANTIATE_TEST_SUITE_P(
   RooNLLVar, OffsetBinTest,
   testing::Combine(testing::Values("Off", "Cpu"), // BatchMode
                    testing::Values(true),         // unbinned or binned (we don't support unbinned fits yet)
                    testing::Values(false, true),  // extended fit
                    testing::Values(false, true)   // use sumW2
                    ),
   [](testing::TestParamInfo<OffsetBinTest::ParamType> const &paramInfo) {
      std::stringstream ss;
      ss << "BatchMode" << std::get<0>(paramInfo.param);
      ss << (std::get<1>(paramInfo.param) ? "Binned" : "Unbinned");
      ss << (std::get<2>(paramInfo.param) ? "Extended" : "");
      ss << (std::get<3>(paramInfo.param) ? "SumW2" : "");
      return ss.str();
   });
