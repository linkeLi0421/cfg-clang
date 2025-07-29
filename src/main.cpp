#include "main.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include <map>
#include <set>
#include <vector>

struct DefUseInfo {
    std::string varName;
    unsigned defLine;
    unsigned useLine;
    std::string context;
    bool isDef;
};

struct CFGBlockInfo {
    unsigned blockId;
    unsigned startLine;
    unsigned endLine;
    std::vector<unsigned> preds;
    std::vector<unsigned> succs;
    std::vector<DefUseInfo> defUseChain;
};

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
	public:
		MyASTVisitor(ASTContext &C, Rewriter &R) : TheContext(C), TheRewriter(R) {}

		bool VisitStmt(Stmt *s) {
			return true;
		}

		void analyzeDefUse(const Stmt* stmt, const SourceManager& SM, 
		                  std::vector<DefUseInfo>& defUseChain, unsigned currentLine) {
			if (!stmt) return;
			
			// Use a set to track unique def-use entries to avoid duplicates
			static std::set<std::string> processedEntries;
			
			if (const DeclStmt* declStmt = dyn_cast<DeclStmt>(stmt)) {
				for (const Decl* decl : declStmt->decls()) {
					if (const VarDecl* varDecl = dyn_cast<VarDecl>(decl)) {
						std::string key = "DEF_" + varDecl->getNameAsString() + "_" + std::to_string(currentLine) + "_declaration";
						if (processedEntries.find(key) == processedEntries.end()) {
							DefUseInfo info;
							info.varName = varDecl->getNameAsString();
							info.defLine = currentLine;
							info.useLine = currentLine;
							info.context = "variable_declaration";
							info.isDef = true;
							defUseChain.push_back(info);
							processedEntries.insert(key);
						}
					}
				}
			}
			
			if (const BinaryOperator* binOp = dyn_cast<BinaryOperator>(stmt)) {
				if (binOp->isAssignmentOp()) {
					const Expr* lhs = binOp->getLHS();
					if (const DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(lhs->IgnoreParenImpCasts())) {
						std::string key = "DEF_" + declRef->getNameInfo().getAsString() + "_" + std::to_string(currentLine) + "_assignment";
						if (processedEntries.find(key) == processedEntries.end()) {
							DefUseInfo info;
							info.varName = declRef->getNameInfo().getAsString();
							info.defLine = currentLine;
							info.useLine = currentLine;
							info.context = "assignment";
							info.isDef = true;
							defUseChain.push_back(info);
							processedEntries.insert(key);
						}
					}
				}
				
				const Expr* rhs = binOp->getRHS();
				if (rhs) {
					analyzeDefUse(rhs, SM, defUseChain, currentLine);
				}
			}
			
			// For variable uses, only record unique uses per variable per line
			if (const DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(stmt)) {
				if (const VarDecl* varDecl = dyn_cast<VarDecl>(declRef->getDecl())) {
					std::string key = "USE_" + varDecl->getNameAsString() + "_" + std::to_string(currentLine) + "_use";
					if (processedEntries.find(key) == processedEntries.end()) {
						DefUseInfo info;
						info.varName = varDecl->getNameAsString();
						info.defLine = 0;
						info.useLine = currentLine;
						info.context = "variable_use";
						info.isDef = false;
						defUseChain.push_back(info);
						processedEntries.insert(key);
					}
				}
			}
			
			// Continue traversal but don't create duplicates
			for (const Stmt* child : stmt->children()) {
				analyzeDefUse(child, SM, defUseChain, currentLine);
			}
		}

		bool VisitFunctionDecl(FunctionDecl *f) {
			if (f->hasBody()) {
				std::string signature = f->getReturnType().getAsString() + " " + f->getNameInfo().getAsString() + "(";
				for (unsigned i = 0; i < f->getNumParams(); ++i) {
					if (i > 0) signature += ", ";
					QualType qt = f->getParamDecl(i)->getType();
					std::string paramType = qt.getAsString();
					std::string paramName = f->getParamDecl(i)->getNameAsString();
					signature += paramType + " " + paramName;
				}
				signature += ")";

				Stmt *funcBody = f->getBody();
				std::unique_ptr<CFG> sourceCFG = CFG::buildCFG(f, funcBody, &TheContext, CFG::BuildOptions());
				const SourceManager &SM = TheContext.getSourceManager();
				SourceLocation fun_loc = f->getLocation();
				if (fun_loc.isValid()) {
					PresumedLoc PLoc = SM.getPresumedLoc(fun_loc);
					if (strncmp(PLoc.getFilename(), "/usr", 4) == 0) {
						return false;
					}
					llvm::outs() << "Function signature: " << signature << "\n";
					llvm::outs() << "Defined in file: " << PLoc.getFilename() << " at line " << PLoc.getLine() << "\n";
				}

				std::vector<CFGBlockInfo> blockInfos;
				unsigned blockId = 0;

				for (const CFGBlock *block : *sourceCFG) {
					CFGBlockInfo blockInfo;
					blockInfo.blockId = blockId++;
					std::optional<unsigned> minLine;
    				std::optional<unsigned> maxLine;
					
					// Get predecessors
					for (const CFGBlock* pred : block->preds()) {
						if (pred && pred->getBlockID() < sourceCFG->size()) {
							blockInfo.preds.push_back(pred->getBlockID());
						}
					}
					
					// Get successors
					for (const CFGBlock* succ : block->succs()) {
						if (succ && succ->getBlockID() < sourceCFG->size()) {
							blockInfo.succs.push_back(succ->getBlockID());
						}
					}

					// Analyze statements in block
					for (const auto &elem : *block) {
						if (elem.getKind() == clang::CFGElement::Statement) {
							const clang::Stmt *stmt = elem.castAs<clang::CFGStmt>().getStmt();
							if (!stmt) continue;

							clang::SourceLocation startLoc = stmt->getBeginLoc();
							clang::SourceLocation endLoc = stmt->getEndLoc();

							if (startLoc.isValid()) {
								PresumedLoc PLoc = SM.getPresumedLoc(startLoc);
								unsigned line = PLoc.getLine();
								minLine = minLine ? std::min(*minLine, line) : line;
								
								// Analyze def-use for this statement
								analyzeDefUse(stmt, SM, blockInfo.defUseChain, line);
							}
							
							if (endLoc.isValid()) {
								PresumedLoc PLoc = SM.getPresumedLoc(endLoc);
								unsigned line = PLoc.getLine();
								maxLine = maxLine ? std::max(*maxLine, line) : line;
							}
						}
					}

					if (minLine && maxLine) {
						blockInfo.startLine = *minLine;
						blockInfo.endLine = *maxLine;
					}
					
					blockInfos.push_back(blockInfo);
				}

				// Print enhanced CFG with def-use information
				for (const auto& blockInfo : blockInfos) {
					llvm::outs() << "[B" << blockInfo.blockId << "]\n";
					
					// Print predecessors
					if (!blockInfo.preds.empty()) {
						llvm::outs() << "  Preds (" << blockInfo.preds.size() << "): ";
						for (size_t i = 0; i < blockInfo.preds.size(); ++i) {
							if (i > 0) llvm::outs() << " ";
							llvm::outs() << "B" << blockInfo.preds[i];
						}
						llvm::outs() << "\n";
					}
					
					// Print successors
					if (!blockInfo.succs.empty()) {
						llvm::outs() << "  Succs (" << blockInfo.succs.size() << "): ";
						for (size_t i = 0; i < blockInfo.succs.size(); ++i) {
							if (i > 0) llvm::outs() << " ";
							llvm::outs() << "B" << blockInfo.succs[i];
						}
						llvm::outs() << "\n";
					}
					
					// Print def-use chain
					if (!blockInfo.defUseChain.empty()) {
						llvm::outs() << "  Def-Use Chain:\n";
						for (const auto& defUse : blockInfo.defUseChain) {
							llvm::outs() << "    " << (defUse.isDef ? "DEF" : "USE") << " " << defUse.varName;
							if (defUse.isDef) {
								llvm::outs() << " at line " << defUse.defLine;
							} else {
								llvm::outs() << " used at line " << defUse.useLine;
							}
							llvm::outs() << " (" << defUse.context << ")\n";
						}
					}
					
					llvm::outs() << "from line " << blockInfo.startLine << " to line " << blockInfo.endLine << "\n\n";
				}
			}

			return true;
		}

	private:
		ASTContext &TheContext;
		Rewriter &TheRewriter;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
	public:
		MyASTConsumer(ASTContext &C, Rewriter &R) : Visitor(C, R) {}

		// Override the method that gets called for each parsed top-level
		// declaration.
		bool HandleTopLevelDecl(DeclGroupRef DR) override {
			for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
				// Traverse the declaration using our AST visitor.
				Visitor.TraverseDecl(*b);
				//(*b)->dump();
			}
			return true;
		}

	private:
		MyASTVisitor Visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
	public:
		MyFrontendAction() {}

		std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
			TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
			return std::make_unique<MyASTConsumer>(CI.getASTContext(), TheRewriter);
		}

	private:
		Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
	auto op = clang::tooling::CommonOptionsParser::create(argc, argv, ToolingSampleCategory);
	if (!op) {
		// Handle the error, for example by printing the message to stderr
		llvm::errs() << op.takeError();
		return 1;
	}
	ClangTool Tool(op->getCompilations(), op->getSourcePathList());

	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}

