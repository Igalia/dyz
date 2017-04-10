local ok, err = pcall(require, "main")
if not ok then
    print(err)
    os.exit(2)
end
