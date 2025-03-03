import { create, isFunc, clTH1I, clTH2I, clTObjString, clTHashList, kNoZoom } from '../core.mjs';
import { DrawOptions } from '../base/BasePainter.mjs';
import { ObjectPainter } from '../base/ObjectPainter.mjs';
import { TH1Painter, PadDrawOptions } from './TH1Painter.mjs';
import { TGraphPainter } from './TGraphPainter.mjs';


/**
 * @summary Painter for TMultiGraph object.
 *
 * @private
 */

class TMultiGraphPainter extends ObjectPainter {

   /** @summary Create painter
     * @param {object|string} dom - DOM element for drawing or element id
     * @param {object} obj - TMultiGraph object to draw */
   constructor(dom, mgraph) {
      super(dom, mgraph);
      this.firstpainter = null;
      this.autorange = false;
      this.painters = []; // keep painters to be able update objects
   }

   /** @summary Cleanup multigraph painter */
   cleanup() {
      this.painters = [];
      super.cleanup();
   }

   /** @summary Update multigraph object */
   updateObject(obj) {
      if (!this.matchObjectType(obj)) return false;

      let mgraph = this.getObject(),
          graphs = obj.fGraphs,
          pp = this.getPadPainter();

      mgraph.fTitle = obj.fTitle;

      let isany = false;
      if (this.firstpainter) {
         let histo = obj.fHistogram;
         if (this.autorange && !histo)
            histo = this.scanGraphsRange(graphs);

         if (this.firstpainter.updateObject(histo))
            isany = true;
      }

      for (let i = 0; i < graphs.arr.length; ++i)
         if ((i < this.painters.length) && this.painters[i].updateObject(graphs.arr[i]))
            isany = true;

      obj.fFunctions?.arr?.forEach(func => {
         if (func?._typename && func?.fName)
            pp?.findPainterFor(null, func.fName, func._typename)?.updateObject(func);
      });

      return isany;
   }

   /** @summary Scan graphs range
     * @return {object} histogram for axes drawing */
   scanGraphsRange(graphs, histo, pad) {
      let mgraph = this.getObject(),
          maximum, minimum, logx = false, logy = false,
          time_display = false, time_format = '',
          rw = { xmin: 0, xmax: 0, ymin: 0, ymax: 0, first: true };

      if (pad) {
         logx = pad.fLogx;
         logy = pad.fLogy;
         rw.xmin = pad.fUxmin;
         rw.xmax = pad.fUxmax;
         rw.ymin = pad.fUymin;
         rw.ymax = pad.fUymax;
         rw.first = false;
      }

      // ignore existing histo in 3d case
      if (this._3d && histo && !histo.fXaxis.fLabels)
         histo = null;

      if (!histo) {
         this.autorange = true;

         if (graphs.arr[0]?.fHistogram?.fXaxis?.fTimeDisplay) {
            time_display = true;
            time_format = graphs.arr[0].fHistogram.fXaxis.fTimeFormat;
         }
      }

      graphs.arr.forEach(gr => {
         if (gr.fNpoints == 0) return;
         if (rw.first) {
            rw.xmin = rw.xmax = gr.fX[0];
            rw.ymin = rw.ymax = gr.fY[0];
            rw.first = false;
         }
         for (let i = 0; i < gr.fNpoints; ++i) {
            rw.xmin = Math.min(rw.xmin, gr.fX[i]);
            rw.xmax = Math.max(rw.xmax, gr.fX[i]);
            rw.ymin = Math.min(rw.ymin, gr.fY[i]);
            rw.ymax = Math.max(rw.ymax, gr.fY[i]);
         }
      });

      if (rw.xmin == rw.xmax)
         rw.xmax += 1.;
      if (rw.ymin == rw.ymax)
         rw.ymax += 1.;
      let dx = 0.05 * (rw.xmax - rw.xmin),
          dy = 0.05 * (rw.ymax - rw.ymin),
          uxmin = rw.xmin - dx,
          uxmax = rw.xmax + dx;
      if (logy) {
         if (rw.ymin <= 0)
            rw.ymin = 0.001 * rw.ymax;
         minimum = rw.ymin / (1 + 0.5 * Math.log10(rw.ymax / rw.ymin));
         maximum = rw.ymax * (1 + 0.2 * Math.log10(rw.ymax / rw.ymin));
      } else {
         minimum = rw.ymin - dy;
         maximum = rw.ymax + dy;
      }
      if (minimum < 0 && rw.ymin >= 0)
         minimum = 0;
      if (maximum > 0 && rw.ymax <= 0)
         maximum = 0;

       let glob_minimum = minimum, glob_maximum = maximum;

      if (uxmin < 0 && rw.xmin >= 0)
         uxmin = logx ? 0.9 * rw.xmin : 0;
      if (uxmax > 0 && rw.xmax <= 0)
         uxmax = logx? 1.1 * rw.xmax : 0;

      if (mgraph.fMinimum != kNoZoom)
         rw.ymin = minimum = mgraph.fMinimum;
      if (mgraph.fMaximum != kNoZoom)
         rw.ymax = maximum = mgraph.fMaximum;

      if (minimum < 0 && rw.ymin >= 0 && logy)
         minimum = 0.9 * rw.ymin;
      if (maximum > 0 && rw.ymax <= 0 && logy)
         maximum = 1.1 * rw.ymax;
      if (minimum <= 0 && logy)
         minimum = 0.001 * maximum;
      if (!logy && minimum > 0 && minimum < 0.05*maximum)
         minimum = 0;
      if (uxmin <= 0 && logx)
         uxmin = (uxmax > 1000) ? 1 : 0.001 * uxmax;

      // Create a temporary histogram to draw the axis (if necessary)
      if (!histo) {
         let xaxis, yaxis;
         if (this._3d) {
            histo = create(clTH2I);
            xaxis = histo.fXaxis;
            xaxis.fXmin = 0;
            xaxis.fXmax = graphs.arr.length;
            xaxis.fNbins = graphs.arr.length;
            xaxis.fLabels = create(clTHashList);
            for (let i = 0; i < graphs.arr.length; i++) {
               let lbl = create(clTObjString);
               lbl.fString = graphs.arr[i].fTitle || `gr${i}`;
               lbl.fUniqueID = graphs.arr.length - i; // graphs drawn in reverse order
               xaxis.fLabels.Add(lbl, '');
            }
            xaxis = histo.fYaxis;
            yaxis = histo.fZaxis;
         } else {
            histo = create(clTH1I);
            xaxis = histo.fXaxis;
            yaxis = histo.fYaxis;
         }
         histo.fTitle = mgraph.fTitle;
         if (histo.fTitle.indexOf(';') >= 0) {
            let t = histo.fTitle.split(';');
            histo.fTitle = t[0];
            if (t[1]) xaxis.fTitle = t[1];
            if (t[2]) yaxis.fTitle = t[2];
         }

         xaxis.fXmin = uxmin;
         xaxis.fXmax = uxmax;
         xaxis.fTimeDisplay = time_display;
         if (time_display) xaxis.fTimeFormat = time_format;
      }

      let axis = this._3d ? histo.fZaxis : histo.fYaxis;
      axis.fXmin = Math.min(minimum, glob_minimum);
      axis.fXmax = Math.max(maximum, glob_maximum);
      histo.fMinimum = minimum;
      histo.fMaximum = maximum;

      return histo;
   }

   /** @summary draw speical histogram for axis
     * @return {Promise} when ready */
   async drawAxisHist(histo, hopt) {
      return TH1Painter.draw(this.getDom(), histo, 'AXIS' + hopt);
   }

   /** @summary method draws next function from the functions list  */
   async drawNextFunction(indx) {

      let mgraph = this.getObject();

      if (!mgraph.fFunctions || (indx >= mgraph.fFunctions.arr.length))
         return this;

      return this.getPadPainter().drawObject(this.getDom(), mgraph.fFunctions.arr[indx], mgraph.fFunctions.opt[indx])
                                 .then(() => this.drawNextFunction(indx+1));
   }

   /** @summary Draw graph  */
   async drawGraph(gr, opt /*, pos3d */ ) {
      return TGraphPainter.draw(this.getDom(), gr, opt);
   }

   /** @summary method draws next graph  */
   async drawNextGraph(indx, opt) {

      let graphs = this.getObject().fGraphs, exec = '';

      // at the end of graphs drawing draw functions (if any)
      if (indx >= graphs.arr.length) {
         this._pfc = this._plc = this._pmc = false; // disable auto coloring at the end
         return this.drawNextFunction(0);
      }

      let gr = graphs.arr[indx], o = graphs.opt[indx] || opt || '';

      // if there is auto colors assignment, try to provide it
      if (this._pfc || this._plc || this._pmc) {
         let mp = this.getMainPainter();
         if (isFunc(mp?.createAutoColor)) {
            let icolor = mp.createAutoColor(graphs.arr.length);
            if (this._pfc) { gr.fFillColor = icolor; exec += `SetFillColor(${icolor});;`; }
            if (this._plc) { gr.fLineColor = icolor; exec += `SetLineColor(${icolor});;`; }
            if (this._pmc) { gr.fMarkerColor = icolor; exec += `SetMarkerColor(${icolor});;`; }
         }
      }

      return this.drawGraph(gr, o, graphs.arr.length - indx).then(subp => {
         if (subp) {
            this.painters.push(subp);
            subp._auto_exec = exec;
         }

         return this.drawNextGraph(indx+1, opt);
      });
   }

   /** @summary Draw multigraph object using painter instance
     * @private */
   static async _drawMG(painter, opt) {

      let d = new DrawOptions(opt);

      painter._3d = d.check('3D');
      painter._pfc = d.check('PFC');
      painter._plc = d.check('PLC');
      painter._pmc = d.check('PMC');

      let hopt = '';
      PadDrawOptions.forEach(name => { if (d.check(name)) hopt += ';' + name; });

      let promise = Promise.resolve(true);
      if (d.check('A') || !painter.getMainPainter()) {
          let mgraph = painter.getObject(),
              histo = painter.scanGraphsRange(mgraph.fGraphs, mgraph.fHistogram, painter.getPadPainter()?.getRootPad(true));

         promise = painter.drawAxisHist(histo, hopt).then(ap => {
            painter.firstpainter = ap;
            ap.$secondary = 'hist'; // mark histogram painter as secondary
            if (mgraph.fHistogram) painter.$primary = true; // mark mg painter as primary
         });
      }

      return promise.then(() => {
         painter.addToPadPrimitives();
         return painter.drawNextGraph(0, d.remain());
      });
   }

   /** @summary Draw TMultiGraph object */
   static async draw(dom, mgraph, opt) {
      return TMultiGraphPainter._drawMG(new TMultiGraphPainter(dom, mgraph), opt);
   }

} // class TMultiGraphPainter

export { TMultiGraphPainter };
