<html>
<body>
<h1>PHP form handler</h1>

<?php
echo "<p>form data is: '{$_POST['text']}'</p>";
if (empty($_POST['text'])) {
	echo "<p>Form data is empty, mod_poc test failed!</p>";
} else {
	echo "<p>Test passed!</p>";
}
echo '<p><a href="javascript:history.back();">Go back</a></p>';
?>

</body>
</html>
