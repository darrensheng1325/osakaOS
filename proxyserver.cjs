const express = require('express');
const app = express();
const port = 3000;

app.get(/^(.*)$/, (req, res) => {
    res.type('.html')
    if (req.url.endsWith('.js')) {
        res.type('.js');
    } else if (req.url.endsWith('.css')) {
        res.type('.css');
    } else if (req.url.endsWith('.json')) {
        res.type('.json');
    } else if (req.url.endsWith('.xml')) {
        res.type('.xml');
    } else if (req.url.endsWith('.svg')) {
        res.type('.svg');
    } else if (req.url.endsWith('.png')) {
        res.type('.png');
    } else if (req.url.endsWith('.jpg')) {
        res.type('.jpg');
    } else if (req.url.endsWith('.jpeg')) {
        res.type('.jpeg');
    } else {
        res.type('.html');
    }
	res.send(fetch(decodeURIComponent(req.url.slice(1))).then(response => response.text()));
});

app.listen(port, () => {
	console.log(`Proxy server is running on port ${port}`);
});