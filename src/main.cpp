#include "main.h"
// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
	public:
		MyASTVisitor(ASTContext &C, Rewriter &R) : TheContext(C), TheRewriter(R) {}

		bool VisitStmt(Stmt *s) {
			return true;
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
				llvm::outs() << "Function signature: " << signature << "\n";

				Stmt *funcBody = f->getBody();
				std::unique_ptr<CFG> sourceCFG = CFG::buildCFG(f, funcBody, &TheContext, CFG::BuildOptions());
				const SourceManager &SM = TheContext.getSourceManager();
				for (const CFGBlock *block : *sourceCFG) {
					std::optional<unsigned> minLine;
    				std::optional<unsigned> maxLine;
					unsigned startLine = 0, endLine = 0;
					block->print(llvm::outs(), sourceCFG.get(), LangOptions(), false);

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
							}
							
							if (endLoc.isValid()) {
								PresumedLoc PLoc = SM.getPresumedLoc(endLoc);
								unsigned line = PLoc.getLine();
								maxLine = maxLine ? std::max(*maxLine, line) : line;
							}
						}
					}

					if (minLine && maxLine) {
						llvm::outs() << "from line " << *minLine << " to line " << *maxLine << "\n";
					}
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

