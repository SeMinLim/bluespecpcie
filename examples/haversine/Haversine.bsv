import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import FloatingPoint::*;
import Float32::*;

import Cordic::*;

import TypeConverter::*;
import ArcSin::*;


interface HaversineIfc;
	method Action dataInToRadian(Bit#(32) d);
	method Action dataInPointALat(Bit#(32) d);
	method Action dataInPointALon(Bit#(32) d);
	method Action dataInPointBLat(Bit#(32) d);
	method Action dataInPointBLon(Bit#(32) d);
	method ActionValue#(Bit#(32)) resultOut;
endinterface
(* synthesize *)
module mkHaversine(HaversineIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	// Cycle Counter
	FIFOF#(Bit#(32)) cycleQ <- mkFIFOF;
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	Reg#(Bit#(32)) cycleStart <- mkReg(0);
	Reg#(Bit#(32)) cycleEnd <- mkReg(0);
	rule incCycleCount;
		cycleCount <= cycleCount + 1;
	endrule
	//--------------------------------------------------------------------------------------------
	// Get data from HwMain & Send result to HwMain
	//--------------------------------------------------------------------------------------------
	Reg#(Maybe#(Bit#(32))) toRadian <- mkReg(tagged Invalid);
	FIFO#(Bit#(32)) pointALat <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) pointALon <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) pointBLat <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) pointBLon <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) pointALatRad <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) pointBLatRad <- mkSizedFIFO(16);
	FIFO#(Bit#(32)) resultQ <- mkSizedFIFO(16);
	Reg#(Bit#(32)) resultCnt <- mkReg(0);
	//--------------------------------------------------------------------------------------------
	// Haversine Formula
	//--------------------------------------------------------------------------------------------
	Vector#(2, FpPairIfc#(32)) toRadian_mult <- replicateM(mkFpMult32);
	Vector#(2, FpPairIfc#(32)) distance_sub <- replicateM(mkFpSub32);
	Vector#(2, FpPairIfc#(32)) distance_mult <- replicateM(mkFpMult32);
	Vector#(2, FpPairIfc#(32)) formula_div <- replicateM(mkFpDiv32);
	Vector#(6, FpPairIfc#(32)) formula_mult <- replicateM(mkFpMult32);
	FpPairIfc#(32) formula_add <- mkFpAdd32;
	FpFilterIfc#(32) formula_sqrt <- mkFpSqrt32;

	Vector#(5, FixedToFloat2IntIfc) fixedtofloat2i <- replicateM(mkFixedToFloat2Int);
	Vector#(4, FloatToFixed3IntIfc) floattofixed3i <- replicateM(mkFloatToFixed3Int);
	Vector#(2, CordicSinCosIfc) formula_sin <- replicateM(mkCordicSinCos);
	Vector#(2, CordicSinCosIfc) formula_cos <- replicateM(mkCordicSinCos);
	ArcSinIfc formula_asin <- mkArcSin;

	// lat1 = lat1 * toRadian
	// lat2 = lat2 * toRadian
	rule toRadianLat1_mult;
		toRadian_mult[0].deq;
		pointALatRad.enq(toRadian_mult[0].first);
	endrule
	rule toRadianLat2_mult;
		toRadian_mult[1].deq;
		pointBLatRad.enq(toRadian_mult[1].first);
	endrule
	// dlat = (lat2 - lat1) * toRadian
	// dlon = (lon2 - lon1) * toRadian
	rule distanceLat_sub;
		pointALat.deq;
		pointBLat.deq;
		let a = pointALat.first;
		let b = pointBLat.first;
		distance_sub[0].enq(b, a);
	endrule
	rule distanceLon_sub;
		pointALon.deq;
		pointBLon.deq;
		let a = pointALon.first;
		let b = pointBLon.first;
		distance_sub[1].enq(b, a);
	endrule
	rule distanceLat_mult;
		distance_sub[0].deq;
		distance_mult[0].enq(distance_sub[0].first, fromMaybe(?,toRadian));
	endrule
	rule distanceLon_mult;
		distance_sub[1].deq;
		distance_mult[1].enq(distance_sub[1].first, fromMaybe(?,toRadian));
	endrule

	// f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(lat1) * cos(lat2)
	rule formulaLat_div;
		distance_mult[0].deq;
		let dlat = distance_mult[0].first;
 		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_div[0].enq(dlat, numTwo);	
	endrule
	rule formulaLon_div;
		distance_mult[1].deq;
		let dlon = distance_mult[1].first;
		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_div[1].enq(dlon, numTwo);
	endrule
	rule formulaLat_sin;
		formula_div[0].deq;
		floattofixed3i[0].enq(formula_div[0].first);
	endrule
	rule formulaLat_sin_floattofixed;
		floattofixed3i[0].deq;
		formula_sin[0].enq(floattofixed3i[0].first);
	endrule
	rule formulaLon_sin;
		formula_div[1].deq;
		floattofixed3i[1].enq(formula_div[1].first);
	endrule
	rule formulaLon_sin_floattofixed;
		floattofixed3i[1].deq;
		formula_sin[1].enq(floattofixed3i[1].first);
	endrule
	rule formulaLat1_cos;
		pointALatRad.deq;
		floattofixed3i[2].enq(pointALatRad.first);
	endrule
	rule formulaLat1_cos_floattofixed;
		floattofixed3i[2].deq;
		formula_cos[0].enq(floattofixed3i[2].first);
	endrule
	rule formulaLat2_cos;
		pointBLatRad.deq;
		floattofixed3i[3].enq(pointBLatRad.first);
	endrule
	rule formulaLat2_cos_floattofixed;
		floattofixed3i[3].deq;
		formula_cos[1].enq(floattofixed3i[3].first);
	endrule
	rule formulaLat_sin_fixedtofloat;
		formula_sin[0].deq;
		let sincos = formula_sin[0].first;
		let sin = tpl_1(sincos);
		fixedtofloat2i[0].enq(sin);
	endrule
	rule formulaLon_sin_fixedtofloat;
		formula_sin[1].deq;
		let sincos = formula_sin[1].first;
		let sin = tpl_1(sincos);
		fixedtofloat2i[1].enq(sin);
	endrule
	rule formulaLat1_cos_fixedtofloat;
		formula_cos[0].deq;
		let sincos = formula_cos[0].first;
		let cos = tpl_2(sincos);
		fixedtofloat2i[2].enq(cos);
	endrule
	rule formulaLat2_cos_fixedtofloat;
		formula_cos[1].deq;
		let sincos = formula_cos[1].first;
		let cos = tpl_2(sincos);
		fixedtofloat2i[3].enq(cos);
	endrule
	rule formulaLat_pow;
		fixedtofloat2i[0].deq;
		let s = fixedtofloat2i[0].first;
		formula_mult[0].enq(s, s);
	endrule
	rule formulaLon_pow;
		fixedtofloat2i[1].deq;
		let s = fixedtofloat2i[1].first;
		formula_mult[1].enq(s, s);
	endrule

	rule formulaFinal_1;
		fixedtofloat2i[3].deq;
		fixedtofloat2i[4].deq;
		let cos_lat1 = fixedtofloat2i[3].first;
		let cos_lat2 = fixedtofloat2i[4].first;
		formula_mult[2].enq(cos_lat1, cos_lat2);
	endrule
	rule formulaFinal_2;
		formula_mult[1].deq;
		let pow_dlon = formula_mult[1].first;

		formula_mult[2].deq;
		let formulaFinal1 = formula_mult[2].first;

		formula_mult[3].enq(pow_dlon, formulaFinal1);
	endrule
	rule formulaFinal_3;
		formula_mult[0].deq;
		let pow_dlat = formula_mult[0].first;

		formula_mult[3].deq;
		let formulaFinal2 = formula_mult[3].first;

		formula_add.enq(pow_dlat, formulaFinal2);
	endrule

	// asin(sqrt(f)) * 2 * 6371
	rule formulaFinal_4;
		formula_add.deq;
		let formulaFinal3 = formula_add.first;
		formula_sqrt.enq(formulaFinal3);
	endrule
	rule formulaFinal_5;
		formula_sqrt.deq;
		let formulaFinal4 = formula_sqrt.first;
		formula_asin.enq(formulaFinal4);
	endrule
	rule formulaFinal_5_type_convert;
		formula_asin.deq;
		let d = formula_asin.first;
		fixedtofloat2i[4].enq(d);
	endrule
	rule formulaFinal_6;
		Bit#(32) earthRadius = 32'b01000101110001110001100000000000;
		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_mult[4].enq(earthRadius, numTwo);
	endrule
	rule formulaFinal_7;
		fixedtofloat2i[4].deq;
		let arcsin = fixedtofloat2i[4].first;

		formula_mult[4].deq;
		formula_mult[5].enq(arcsin, formula_mult[4].first);
	endrule
	rule formulaFinal_8;
		formula_mult[5].deq;
		resultQ.enq(formula_mult[5].first);
		resultCnt <= resultCnt + 1;
		$write("\033[1;33mCycle %1d -> \033[1;33m[Haversine]: \033[0m: Computation \033[1;32mdone!\033[0m[%1d]\n",
			cycleCount,resultCnt);	
	endrule

	method Action dataInToRadian(Bit#(32) d);
		toRadian <= tagged Valid d;
	endmethod
	method Action dataInPointALat(Bit#(32) d);
		pointALat.enq(d);
		if ( isValid(toRadian) ) toRadian_mult[0].enq(d, fromMaybe(?,toRadian));
	endmethod
	method Action dataInPointALon(Bit#(32) d);
		pointALon.enq(d);
	endmethod
	method Action dataInPointBLat(Bit#(32) d);
		pointBLat.enq(d);
		if ( isValid(toRadian) ) toRadian_mult[1].enq(d, fromMaybe(?,toRadian));
	endmethod
	method Action dataInPointBLon(Bit#(32) d);
		pointBLon.enq(d);
	endmethod
	method ActionValue#(Bit#(32)) resultOut;
		resultQ.deq;
		return resultQ.first;
	endmethod
endmodule
