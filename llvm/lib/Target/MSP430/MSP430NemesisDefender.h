//
// Created by steffie on 08.02.21.
//

#ifndef LLVM_MSP430NEMESISDEFENDER_H
#define LLVM_MSP430NEMESISDEFENDER_H

#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineLoopInfo.h>
#include <llvm/CodeGen/MachinePostDominators.h>
using namespace llvm;

namespace  {

/// Defends agains Nemesis attacks
class MSP430NemesisDefenderPass : public MachineFunctionPass {
public:

  /// A vector of defs (instruction ids) for a given register unit
  //TODO: using RegUnitDefs = SmallVector<size_t, 1>;
  using RegUnitDefs = std::vector<size_t>;
  /// All defs for a given MBB, indexed by register unit id
  using MBBDefsInfo = std::vector<RegUnitDefs>;

  /// A vector of dependencies to instructions, used for storing reaching
  //   definitions.
  using MIDepsInfo = SmallVector<MachineInstr *, 1>;
  /// All instruction dependencies for a given MBB, indexed by instruction id
  using MBBDepsInfo = std::vector<MIDepsInfo>;

  enum BranchClass {
    BCNotClassified, // MBB is unclassified
    BCFork,          // MBB is the entry of a fork shaped sub-CFG
    BCDiamond,       // MBB is the entry of a diamond shaped sub-CFG
    BCTriangle,      // MBB is the entry of a triangle shaped sub-CFG.
  };

  struct MBBInfo {
    bool IsDone                     : 1;
    bool IsAligned                  : 1;
    bool IsAnalyzable               : 1;
    bool IsBranch                   : 1; // Conditional or unconditional branch
    bool IsConditionalBranch        : 1;
    bool IsPartOfSensitiveRegion    : 1;
    bool IsLoopHeader               : 1;
    bool IsLoopLatch                : 1;
    bool IsCanonicalLoopBlock       : 1;
    bool HasSecretDependentBranch   : 1;
    bool IsEntry                    : 1;
    bool IsReturn                   : 1;
    int TripCount = -1; /* Only relevant when IsLoopHeader is true */
    /* LTODO: Add LoopInfo struct ? */
    size_t TerminatorCount = 0;
    MachineBasicBlock *BB = nullptr;
    MachineBasicBlock *Orig = nullptr; // Orignal contents of BB
    // Next is set when the next block can be statically determined
    MachineBasicBlock *Next = nullptr;
    MachineBasicBlock *TrueBB = nullptr;
    MachineBasicBlock *FalseBB = nullptr;
    MachineBasicBlock *FallThroughBB = nullptr;
    SmallVector<MachineOperand, 4> BrCond;

    // !TODO: Figure out if the implemenation cannot use the
    //         MachineRegisterInfo (MRI) class for this...
    //        (there seems to be some redundancy with what I implemented
    //         and with what this class provides...)
    // => And what about the liveness information, why can't we reuse that?
    MBBDefsInfo Defs;
    MBBDepsInfo Deps;

#if 0
    // TODO: Transform to OO design with polymorhic method align()
    //        and match() class method
    // Branch class info
    BranchClass BClass = BCNotClassified;
    union {
      struct {
        MachineBasicBlock *LeftBB;
        MachineBasicBlock *RightBB;
      } Fork;
      struct {
      } Diamond;
      struct {
        MachineBasicBlock *DivBB;  // Block that diverges from the 'short path'
        MachineBasicBlock *JoinBB; // Block where the branches rejoin
      } Triangle;
    } BCInfo;
#endif

    MBBInfo() : IsDone(false), IsAligned(false), IsAnalyzable(false),
                IsBranch(false), IsConditionalBranch(false),
                IsPartOfSensitiveRegion(false), IsLoopHeader(false), IsLoopLatch(false),
                IsCanonicalLoopBlock(false),
                HasSecretDependentBranch(false),
                IsEntry(false), IsReturn(false) {}
  };

  // Set to true when the sensitivity analysis detected at least one
  // secret dependent branch
  bool HasSecretDependentBranch = false;

  // Return type of ComputeSuccessors
  struct Successors {
    std::vector<MachineBasicBlock *> Succs;
    MachineLoop *Loop; // ! TODO: Beware of dangling pointers
  };

  MSP430NemesisDefenderPass() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "MSP430 Nemesis Defender"; }
  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  void releaseMemory() override;

  /// Pass identification, replacement for typeid.
  static char ID;

private:

  /// Maps instructions to their instruction Ids, relative to the beginning of
  /// their basic blocks.
  DenseMap<MachineInstr *, size_t> InstIds;
  /// The set of sensitive instructions
  SmallPtrSet<const MachineInstr *, 10> SensitivityInfo;

  MachineFunction *MF;
  // The sensitivity analysis procedure determines whether canonicalization is
  // required (i.e. when a sensitive region contains a return node).
  MachineBasicBlock *CanonicalExit = nullptr;
  MachineRegisterInfo *MRI;
  MachineLoopInfo *MLI;
  //const TargetLoweringBase *TLI;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  MachineDominatorTree *MDT;
  MachinePostDominatorTree *MPDT;

  /// TODO: OPTIMIZE: Analysis results indexed by basic block number
  //         SmallVector<MBBInfo, 4> BBAnalysis;
  std::map<MachineBasicBlock *, MBBInfo> BBAnalysis;
  MBBInfo *EntryBBI = nullptr;

  MBBInfo *GetInfo(MachineBasicBlock &MMB);
  std::vector<size_t> GetDefs(MBBInfo *BBI, size_t RU,
                              std::function<bool(size_t)> P);
  std::vector<size_t> GetDefsBefore(MBBInfo *BBI, size_t RU, size_t IID);
  std::vector<size_t> GetDefsAfter(MBBInfo *BBI, size_t RU, size_t IID);
  MachineBasicBlock *CreateMachineBasicBlock(
      StringRef debug, bool addToMF=false);
  MachineBasicBlock *CloneMBB(MachineBasicBlock *MBB, bool addToMF);

  void Taint(MachineInstr *MI);
  bool IsPartOfSensitiveRegion(const MachineInstr *MI);

  MachineBasicBlock *GetExitOfSensitiveBranch(MachineBasicBlock *Entry);

  void RemoveTerminationCode(MachineBasicBlock &MBB);
  void ReplaceSuccessor(MachineBasicBlock *MBB, MachineBasicBlock *Old,
                        MachineBasicBlock *New);

  std::vector<MachineBasicBlock *> GetFingerprint(MachineLoop *L);
  void BuildFingerprint(MachineLoop *L, std::vector<MachineBasicBlock *> &FP);

  // Exit is the "join block" or the "point of convergence" of the originating
  //  sensitive region.
  Successors
  ComputeSuccessors(std::vector<MachineBasicBlock *> L, MachineBasicBlock *Exit);

  void AlignNonTerminatingInstructions(std::vector<MachineBasicBlock *> L);
  void CanonicalizeTerminatingInstructions(MachineBasicBlock *MBB);
  void AlignTwoWayBranch(MachineBasicBlock &MBB);

  // Returns information about sensitivity of machine instructions and basic
  //  block info.
  bool IsSecretDependent(MachineInstr *MI);
  bool IsSecretDependent(MBBInfo *BBI);
  bool IsTwoWayBranch(MBBInfo *BBI);

  void PrepareAnalysis();
  void FinishAnalysis();
  void CanonicalizeCFG();

  bool addDependency(MachineInstr *MI, MachineBasicBlock *MBB,
                     std::vector<size_t> &Defs);

  // RU is a register unit
  void ComputeDependencies(MachineInstr *MI, size_t RU, MachineBasicBlock *MBB,
                           SmallPtrSetImpl<MachineBasicBlock *> &Visited);

  void RegisterDefs(MBBInfo &BBI);

  virtual void CompensateInstr(const MachineInstr &MI, MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI);
  void CompensateCall(const MachineInstr &Call, MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator MBBI);
  void SecureCall(MachineInstr &Call);

#if 0
  MachineBasicBlock::iterator GetPosBeforeBranchingCode(MachineBasicBlock *MBB)
  const;

  bool MatchFork(MBBInfo &EBBI);
  bool MatchDiamond(MBBInfo &EBBI);
  bool MatchTriangle(MBBInfo &EBBI, bool DivOnFalse);

  MachineBasicBlock::iterator AlignBlock(MachineBasicBlock &Source,
                                         MachineBasicBlock::iterator SI,
                                         MachineBasicBlock &Target,
                                         MachineBasicBlock::iterator TI);

  bool IsEnryOfPattern(MBBInfo &BBI, BranchClass BClass);

  void AlignTriangle(MBBInfo &EBBI);
  void AlignDiamond(MBBInfo &EBBI);
  void AlignFork(MBBInfo &EBBI);

  void ClassifyBranches();
#endif

  // TODO: Move analyzeCompare to TargetInstrInfo?
  bool analyzeCompare(const MachineInstr &MI, unsigned &SrcReg,
                      unsigned &SrcReg2, int &CmpMask, int &CmpValue) const;
  int GetLoopTripCount(MachineLoop *L);

  void RedoAnalysisPasses();

  void AnalyzeControlFlow(MBBInfo &BBI);
  void AnalyzeControlFlow(MachineBasicBlock &MBB);
  void AnalyzeControlFlow();

  void ReAnalyzeControlFlow(MachineBasicBlock &MBB);

  void VerifyControlFlowAnalysis();

  void ComputeReachingDefs();
  void PeformSensitivityAnalysis();

  void DetectOuterSensitiveBranches();
  void AnalyzeLoops();

  void CanonicalizeSensitiveLoop(MachineLoop *Loop);
  void AlignContainedRegions(MachineLoop *L);

  void SecureCalls();
  void AlignSensitiveBranches();

  void AlignSensitiveBranch(MBBInfo &BBI);
  std::vector<MachineBasicBlock *>
  AlignSensitiveLoop(MachineLoop *Loop, std::vector<MachineBasicBlock *> MBBs);
  std::vector<MachineBasicBlock *>
  AlignFingerprint(std::vector<MachineBasicBlock *> FP, std::vector<MachineBasicBlock *> MBBs);

  void WriteCFG(std::string label);
  void DumpCFG();
  void DumpDebugInfo();
};

} // end anonymous namespace
#endif // LLVM_MSP430NEMESISDEFENDER_H
