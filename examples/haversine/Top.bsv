import Clocks::*;
import ClockImport::*;
import DefaultValue::*;
import FIFO::*;
import Vector::*;
import Connectable::*;

// PCIe stuff
import PcieImport::*;
import PcieCtrl::*;
import PcieCtrl_bsim::*;

import HwMain::*;


interface TopIfc;
	(* always_ready *)
	interface PcieImportPins pcie_pins;
	(* always_ready *)
	method Bit#(4) led;
endinterface

(* no_default_clock, no_default_reset *)
module mkProjectTop #(
	Clock pcie_clk_p, Clock pcie_clk_n, Clock emcclk,
	Clock sys_clk_p, Clock sys_clk_n,
	Reset pcie_rst_n
	) 
		(TopIfc);


	PcieImportIfc pcie <- mkPcieImport(pcie_clk_p, pcie_clk_n, pcie_rst_n, emcclk);
	Clock pcie_clk_buf = pcie.sys_clk_o;
	Reset pcie_rst_n_buf = pcie.sys_rst_n_o;

	ClockGenIfc clk_200mhz_import <- mkClockIBUFDSImport(sys_clk_p, sys_clk_n);
	Clock sys_clk_200mhz = clk_200mhz_import.gen_clk;
	ClockGenIfc sys_clk_200mhz_buf_import <- mkClockBUFGImport(clocked_by sys_clk_200mhz);
	Clock sys_clk_200mhz_buf = sys_clk_200mhz_buf_import.gen_clk;
	Reset rst200 <- mkAsyncReset( 4, pcie_rst_n, sys_clk_200mhz_buf);

	PcieCtrlIfc pcieCtrl <- mkPcieCtrl(pcie.user, clocked_by pcie.user_clk, reset_by pcie.user_reset);

	HwMainIfc hwmain <- mkHwMain(pcieCtrl.user, clocked_by sys_clk_200mhz_buf, reset_by rst200);


	// Interfaces ////
	interface PcieImportPins pcie_pins = pcie.pins;

	method Bit#(4) led;
		//return leddata;
		return 0;
	endmethod
endmodule

module mkProjectTop_bsim (Empty);
	Clock curclk <- exposeCurrentClock;

	PcieCtrlIfc pcieCtrl <- mkPcieCtrl_bsim;

	HwMainIfc hwmain <- mkHwMain(pcieCtrl.user);
endmodule
