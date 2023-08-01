package DRAMArbiter;


import FIFO::*;
import FIFOF::*;
import Vector::*;
import MergeN::*;
import DRAMController::*;

import BRAMFIFO::*;


interface DRAMArbiterUserIfc;
	method Action cmd(Bit#(64) addr, Bit#(32) words, Bool write);
	method ActionValue#(Bit#(512)) read;
	method Action write(Bit#(512) data);
endinterface
interface DRAMArbiterIfc#(numeric type ways);
	interface Vector#(ways, DRAMArbiterUserIfc) users;
endinterface
module mkDRAMArbiter#(DRAMUserIfc dram) (DRAMArbiterIfc#(ways));
	
	MergeNIfc#(ways, Tuple4#(Bit#(8), Bit#(64), Bit#(32), Bool)) cmdQ <- mkMergeN;
	Vector#(ways, FIFO#(Bit#(512))) rDataQ <- replicateM(mkSizedBRAMFIFO(512));
	Vector#(ways, FIFO#(Bit#(512))) wDataQ <- replicateM(mkSizedBRAMFIFO(512));

	FIFO#(Bit#(8)) reqQ <- mkSizedBRAMFIFO(256);

	Reg#(Bit#(8)) curActiveWay <- mkReg(0);
	Reg#(Bit#(64)) curAddr <- mkReg(0);
	Reg#(Bit#(32)) curWordLeft <- mkReg(0);
	Reg#(Bool) curCmdIsWrite <- mkReg(?);
	rule getCmd ( curWordLeft == 0 );
		cmdQ.deq;
		let c = cmdQ.first;
		
		let actway = tpl_1(c);
		let addr = tpl_2(c);
		let words = tpl_3(c);
		let write = tpl_4(c);

		if ( write ) begin
			curActiveWay <= actway;
			curAddr <= addr;
			curWordLeft <= words;
			curCmdIsWrite <= write;	
		end else begin
			dram.readReq(addr, 7'b1000000);
			reqQ.enq(actway);
			
			curActiveWay <= actway;
			curAddr <= addr + 64;
			curWordLeft <= words - 1;
			curCmdIsWrite <= write;
		end
	endrule

	rule doWrite( curCmdIsWrite == True && curWordLeft > 0 );
		dram.write(curAddr, wDataQ[curActiveWay].first, 7'b1000000);
		wDataQ[curActiveWay].deq;
		curAddr <= curAddr + 64;
		curWordLeft <= curWordLeft - 1;
	endrule

	rule doReadReq( curCmdIsWrite == False && curWordLeft > 0 );
		dram.readReq(curAddr, 7'b1000000);
		reqQ.enq(curActiveWay);
		curAddr <= curAddr + 64;
		curWordLeft <= curWordLeft - 1;
	endrule

	rule doRead;
		let d <- dram.read;

		reqQ.deq;
		let actway = reqQ.first;

		rDataQ[actway].enq(d);
	endrule

	Vector#(ways, DRAMArbiterUserIfc) users_;
	for (Integer i = 0; i < valueOf(ways); i = i + 1 ) begin
		users_[i] = interface DRAMArbiterUserIfc;
		method Action cmd(Bit#(64) addr, Bit#(32) words, Bool write);
			cmdQ.enq[i].enq(tuple4(fromInteger(i), addr, words, write));
		endmethod
		method ActionValue#(Bit#(512)) read;
			rDataQ[i].deq;
			return rDataQ[i].first;
		endmethod
		method Action write(Bit#(512) data);
			wDataQ[i].enq(data);
		endmethod
		endinterface: DRAMArbiterUserIfc;
	end
	interface users = users_;
endmodule

endpackage: DRAMArbiter
