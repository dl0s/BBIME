import com.rim.tad.tools.qml.parser.QMLParserUtil;
import com.rim.tad.tools.qml.parser.ast.ASTParserError;
import com.rim.tad.tools.qml.parser.ast.ASTQML;
import com.rim.tad.tools.qml.parser.ast.ASTArray;
import com.rim.tad.tools.qml.parser.ast.ASTBlock;
import com.rim.tad.tools.qml.parser.ast.ASTElement;
import com.rim.tad.tools.qml.parser.ast.ASTExpression;
import com.rim.tad.tools.qml.parser.ast.ASTQMLNamedValue;
import com.rim.tad.tools.qml.parser.ast.ASTQMLObject;
import com.rim.tad.tools.qml.parser.ast.ASTStatementExpression;
import com.rim.tad.tools.qml.parser.ast.ASTVisitor;
import com.rim.tad.tools.qml.parser.ast.StringBuilderParserContext;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** SDK IDE grammar plus a narrow guard for Q10-rejected QML binding semicolons. */
public final class verify_qml_syntax {
    private static int skipTrivia(String source, int offset) {
        while (offset < source.length()) {
            if (Character.isWhitespace(source.charAt(offset))) { ++offset; continue; }
            if (source.startsWith("//", offset)) {
                int next = source.indexOf('\n', offset + 2);
                return next < 0 ? source.length() : skipTrivia(source, next + 1);
            }
            if (source.startsWith("/*", offset)) {
                int next = source.indexOf("*/", offset + 2);
                return next < 0 ? source.length() : skipTrivia(source, next + 2);
            }
            break;
        }
        return offset;
    }

    private static void rejectBindingSemicolon(String source, ASTElement expression,
            List<ASTParserError> errors, String kind) {
        int offset = skipTrivia(source, expression.getStopCharIndex());
        if (offset < source.length() && source.charAt(offset) == ';') {
            errors.add(new ASTParserError(offset, offset + 1,
                    "Unexpected token ';' after QML " + kind + " (Q10 compatibility guard)"));
        }
    }

    private static List<ASTParserError> parse(final String source) {
        final List<ASTParserError> errors = new ArrayList<ASTParserError>();
        ASTQML document = QMLParserUtil.parse(errors,
                new StringBuilderParserContext(new StringBuilder(source)));
        if (document == null && errors.isEmpty()) {
            throw new IllegalStateException("SDK parser returned no document or diagnostic");
        }
        // The IDE grammar tolerates array-binding terminators rejected by the
        // device. Inspect QML binding AST nodes only; JavaScript arrays, object
        // literals and function statement terminators are left alone.
        if (document != null) document.accept(new ASTVisitor.Adapter() {
            public ASTVisitor.State visit(ASTQMLNamedValue binding) {
                ASTExpression value = binding.getValue();
                if (value instanceof ASTArray) {
                    ASTArray array = (ASTArray) value;
                    boolean objectArray = array.isEmpty();
                    for (ASTExpression item : array) if (item instanceof ASTQMLObject) objectArray = true;
                    if (objectArray) rejectBindingSemicolon(source, value, errors, "object array binding");
                } else if (value instanceof ASTQMLObject) {
                    rejectBindingSemicolon(source, value, errors, "object binding");
                } else if (value instanceof ASTStatementExpression
                        && ((ASTStatementExpression) value).getStatement() instanceof ASTBlock) {
                    rejectBindingSemicolon(source, value, errors, "block binding");
                }
                return ASTVisitor.State.CONTINUE;
            }
        });
        Collections.sort(errors, new Comparator<ASTParserError>() {
            public int compare(ASTParserError a, ASTParserError b) {
                return Integer.compare(a.getStartOffset(), b.getStartOffset());
            }
        });
        Map<String, ASTParserError> unique = new LinkedHashMap<String, ASTParserError>();
        for (ASTParserError error : errors) {
            unique.put(error.getStartOffset() + ":" + error.getEndOffset() + ":" + error.getMessage(), error);
        }
        return new ArrayList<ASTParserError>(unique.values());
    }

    private static String diagnostic(String name, String source, ASTParserError error) {
        int offset = Math.max(0, Math.min(source.length(), error.getStartOffset()));
        int line = 1, column = 1;
        for (int i = 0; i < offset; ++i) {
            if (source.charAt(i) == '\n') { ++line; column = 1; }
            else ++column;
        }
        return name + ":" + line + ":" + column + ": " + error.getMessage();
    }

    private static int checkFile(Path path) throws IOException {
        String source = new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
        List<ASTParserError> errors = parse(source);
        for (ASTParserError error : errors) System.err.println(diagnostic(path.toString(), source, error));
        if (errors.isEmpty()) System.out.println("QML syntax PASS: " + path.getFileName());
        return errors.size();
    }

    private static void selfTest() {
        String valid = "import bb.cascades 1.4\nPage {\n"
                + "  keyListeners: [ KeyListener { onKeyEvent: {} } ]\n"
                + "  function sample() { var a = []; var b = {}; var c = /[;\\]]/; return a; }\n"
                + "  onCreationCompleted: { var x = []; var y = {}; }\n"
                + "  Container {}\n}\n";
        if (!parse(valid).isEmpty()) throw new IllegalStateException("SDK parser rejected valid QML array fixture");
        String broken = valid.replace("} ]\n", "} ];\n");
        List<ASTParserError> errors = parse(broken);
        int badOffset = broken.indexOf(";\n");
        boolean found = false;
        for (ASTParserError error : errors) {
            if (error.getStartOffset() <= badOffset && error.getEndOffset() > badOffset) found = true;
            System.out.println("EXPECTED rejection: " + diagnostic("array-semicolon.qml", broken, error));
        }
        if (!found) throw new IllegalStateException("SDK parser did not identify forbidden QML array semicolon");
        if (!parse(valid.replace("} ]\n", "} ] /* terminator */ ;\n")).isEmpty()) {
            System.out.println("QML syntax self-test PASS: comment-separated array semicolon rejected");
        } else throw new IllegalStateException("Compatibility guard missed comment-separated semicolon");
        System.out.println("QML syntax self-test PASS: valid QML and JavaScript semicolons accepted; forbidden array semicolon rejected");
    }

    public static void main(String[] args) {
        try {
            int failures = 0, files = 0;
            for (String arg : args) {
                if (arg.equals("--self-test")) { selfTest(); continue; }
                failures += checkFile(Paths.get(arg));
                ++files;
            }
            if (args.length == 0) throw new IllegalArgumentException("Supply QML file paths or --self-test");
            if (failures != 0) {
                System.err.println("QML syntax FAILED: " + failures + " diagnostic(s) across " + files + " file(s)");
                System.exit(1);
            }
            if (files > 0) System.out.println("QML syntax PASS: " + files + " file(s); syntax only, runtime loading still requires Q10");
        } catch (Exception error) {
            System.err.println("QML syntax verifier error: " + error);
            System.exit(2);
        } catch (LinkageError error) {
            System.err.println("QML syntax verifier SDK dependency error: " + error);
            System.exit(2);
        }
    }
}
